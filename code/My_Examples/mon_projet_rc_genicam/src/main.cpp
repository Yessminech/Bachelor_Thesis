#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/stream.h>
#include <rc_genicam_api/buffer.h>
#include <rc_genicam_api/image.h>
#include <rc_genicam_api/image_store.h>
#include <rc_genicam_api/nodemap_out.h>
#include <opencv2/opencv.hpp> // OpenCV for visualization

#include <iostream>
#include <atomic>
#include <thread>
#include <signal.h>
#include <iomanip>
#include <memory>
#include <string>
#include <stdexcept>
#include <chrono>

std::atomic<bool> stop_streaming(false);
std::shared_ptr<rcg::Stream> stream;

// Function to handle termination signals (Ctrl+C)
void signalHandler(int)
{
    std::cout << "\nStopping stream..." << std::endl;
    stop_streaming = true;
}

// Function to enable Chunk Data Mode for metadata extraction
void enableChunkData(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    if (nodemap)
    {
        rcg::setBoolean(nodemap, "ChunkModeActive", true);
        std::cout << "Chunk Data Mode enabled." << std::endl;
    }
}

void enablePTP(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    std::string feature = "PtpEnable";
    std::string value = "true";
    try
    {
        rcg::setString(nodemap, feature.c_str(), value.c_str(), true);
        std::cout << "Feature '" << feature << "' is now set to: " << rcg::getBoolean(nodemap, feature.c_str()) << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Failed to set " << feature << ": - Trying deprecated feature: GevIEEE1588" << std::endl;
        feature = "GevIEEE1588";
        try
        {
            rcg::setString(nodemap, feature.c_str(), value.c_str(), true);
            std::cout << "Feature '" << feature << "' is now set to: " << rcg::getBoolean(nodemap, feature.c_str()) << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Failed to set " << feature << ": " << ex.what() << std::endl;
        }
    }
}

void printTimestamp(std::shared_ptr<GenApi::CNodeMapRef> nodemap, const std::string &deviceID)
{
    try
    {
        rcg::callCommand(nodemap, "TimestampLatch");
        if (rcg::getString(nodemap, "TimestampLatchValue") == "")
        {
            std::cerr << "Failed to get Timestamp from device '" << deviceID << ": - Trying depracted features: GevTimestampControlLatch, GevTimestampValue" << std::endl;

            rcg::callCommand(nodemap, "GevTimestampControlLatch");
            uint64_t timestamp = std::stoull(rcg::getString(nodemap, "GevTimestampValue"));
            double tickFrequency = std::stod(rcg::getString(nodemap, "GevTimestampTickFrequency")); // if PTP is used, this feature must return 1,000,000,000 (1 GHz).
            uint64_t timestamp_s = timestamp / tickFrequency;
            std::cout << "Timestamp on device '" << deviceID << "' is: " << timestamp_s << std::endl;
        }

        else
        {
            std::cout << "Timestamp on device '" << deviceID << "' is: " << rcg::getString(nodemap, "TimestampLatchValue") << "ns." << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Failed to get Timestamp from device '" << deviceID << ": - Trying depracted features: GevTimestampControlLatch, GevTimestampValue" << std::endl;
        try
        {
            rcg::callCommand(nodemap, "GevTimestampControlLatch");
            std::cout << "Timestamp on device '" << deviceID << "' is: " << rcg::getString(nodemap, "GevTimestampValue") << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Failed to get Timestamp from device '" << deviceID << ": " << ex.what() << std::endl;
        }
    }
}

void printPtpConfig(std::shared_ptr<GenApi::CNodeMapRef> nodemap, const std::string &deviceID)
{

    try
    {
        rcg::getString(nodemap, "PtpEnable", true);
        rcg::callCommand(nodemap, "PtpDataSetLatch");
        std::cout << "PTP:                        " << rcg::getString(nodemap, "PtpEnable") << std::endl;
        std::cout << "PTP status:                 " << rcg::getString(nodemap, "PtpStatus") << std::endl;
        std::cout << "PTP offset:                 " << rcg::getInteger(nodemap, "PtpOffsetFromMaster") << " ns" << std::endl;
    }
    catch (const std::exception &)
    {
        std::cerr << "Failed to get PTP configuration from device '" << deviceID << "'- Trying depracted features: GevIEEE1588, GevIEEE1588Status " << std::endl;
        try
        {
            rcg::getString(nodemap, "GevIEEE1588", true);
            std::cout << "PTP:                        " << rcg::getString(nodemap, "GevIEEE1588") << std::endl;
            std::cout << "PTP status:                 " << rcg::getString(nodemap, "GevIEEE1588Status") << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cout << "Ptp parameters are not available" << std::endl;
            std::cout << std::endl;
        }
    }
}

// Waits until all cameras are set to slaves and one camera to master
bool waitForPTPStatus(const std::vector<std::shared_ptr<rcg::Device>> &cameras, bool deprecated, int ptp_sync_timeout)
{
    std::string feature;
    std::cout << "Waiting for " << cameras.size() << " cameras to have correct PTP status.." << std::endl;
    if (deprecated)
    {
        feature = "GevIEEE1588Status";
    }
    else
    {
        feature = "PtpStatus";
    }
    auto timeout_time = std::chrono::steady_clock::now() + std::chrono::seconds(ptp_sync_timeout);
    while (true)
    {
        int num_init = 0;
        int num_master = 0;
        int num_slave = 0;
        int64_t master_clock_id = 0;

        for (const auto &camera : cameras)
        {
            auto nodemap = camera->getRemoteNodeMap();
            std::string current_status = rcg::getString(nodemap, feature.c_str());

            if (current_status == "Master")
            {
                ++num_master;
            }
            else if (current_status == "Slave")
            {
                ++num_slave;
            }
            else
            {
                ++num_init;
            }
        }

        if (num_slave == cameras.size() - 1 && num_master == 1)
        {
            std::cout << "All camera clocks are PTP slaves to master clock: " << master_clock_id << std::endl;
            return true;
        }
        else
        {
            std::cout << "Camera PTP status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
        }

        if (std::chrono::steady_clock::now() > timeout_time)
        {
            std::cerr << "Timed out waiting for camera clocks to become PTP camera slaves. Current status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// Function to set the camera to Sync Free Run (Continuous Acquisition)
void configureSyncFreeRun(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    if (nodemap)
    {
        // ToDo: Add try and catch Exceptions
        rcg::setEnum(nodemap, "AcquisitionMode", "Continuous");
        // Make sure that the Frame Start trigger is set to Off to enable free run
        rcg::setEnum(nodemap, "TriggerSelector", "FrameStart");
        rcg::setEnum(nodemap, "TriggerMode", "Off");
        // Specify a trigger rate of 30 frames per second
        rcg::setBoolean(nodemap, "AcquisitionFrameRateEnable", true);
        rcg::setFloat(nodemap, "AcquisitionFrameRate", 30.0); // Set FPS to 30
        std::cout << "Camera configured for Sync Free Run (Continuous Acquisition) at 30 FPS." << std::endl;
    }
}

// ToDo: Replace with loading from .xml file
// Function to adjust camera settings (e.g., exposure, gain, PixelFormat)
void adjustCameraSettings(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    std::cout << "Adjusting camera settings..." << std::endl;
    if (nodemap)
    {
        rcg::setFloat(nodemap, "ExposureTimeAbs", 20000.0); // Set exposure to 20ms
        rcg::setInteger(nodemap, "GainRaw", 5.0);  
        rcg::setString(nodemap, "PixelFormat", "BayerRG8");
        std::cout << "Camera settings adjusted: Exposure=20ms, Gain=5, PixelFormat=BayerGR12." << std::endl;
    }
}

// Function to extract metadata from each image buffer
void extractMetadata(std::shared_ptr<GenApi::CNodeMapRef> nodemap, const rcg::Buffer *buffer)
{
    if (nodemap && buffer)
    {
        uint64_t timestamp = buffer->getTimestampNS();
        double exposure = rcg::getFloat(nodemap, "ExposureTime");
        double gain = rcg::getFloat(nodemap, "Gain");

        std::cout << "Metadata - Timestamp: " << timestamp << " ns, Exposure: "
                  << exposure << " ms, Gain: " << gain << std::endl;
    }
}

// Function to process and store images in PNG/PNM format
void storeImage(const rcg::Buffer *buffer, const std::string &format = "png")
{
    if (!buffer || buffer->getIsIncomplete())
    {
        std::cerr << "Received incomplete buffer, skipping storage..." << std::endl;
        return;
    }

    std::ostringstream filename;
    filename << "image_" << buffer->getTimestampNS() << "." << format;

    rcg::Image image(buffer, 0); // Assume first part contains the image
    rcg::storeImage(filename.str(), format == "png" ? rcg::PNG : rcg::PNM, image);

    std::cout << "Image stored: " << filename.str() << std::endl;
}

// Function to convert and display images using OpenCV
void displayImage(const rcg::Buffer *buffer)
{
    if (!buffer || buffer->getIsIncomplete())
    {
        std::cerr << "Received incomplete buffer, skipping display..." << std::endl;
        return;
    }

    // Convert the buffer to an OpenCV Mat
    rcg::Image image(buffer, 0); // Assume first part contains the image

    cv::Mat frame(image.getHeight(), image.getWidth(), CV_8UC1, (void *)image.getPixels());

    // Convert grayscale to BGR for display
    cv::Mat display_frame;
    cv::cvtColor(frame, display_frame, cv::COLOR_BayerBG2BGR );
    cv::flip(display_frame, display_frame, 1); // Flip vertically
    cv::resize(display_frame, display_frame, cv::Size(), 0.2, 0.2);

    // Show the image
    cv::imshow("Live Stream", display_frame);

    // Press 'ESC' to stop streaming
    if (cv::waitKey(1) == 27)
    {
        stop_streaming = true;
    }
}

// Function to stop streaming
void stopStreaming(std::shared_ptr<rcg::Device> device)
{
    if (stream)
    {
        stream->stopStreaming();
        stream->close();
        std::cout << "Streaming stopped." << std::endl;
    }

    if (device)
    {
        device->close();
        std::cout << "Device closed." << std::endl;
    }

    rcg::System::clearSystems();
}

// Function to start streaming
bool startStreaming(std::shared_ptr<rcg::Device> device)
{
    try
    {
        std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();
        adjustCameraSettings(nodemap); // Apply exposure and gain settings
        configureSyncFreeRun(nodemap); // Set to Sync Free Run mode
        enablePTP(nodemap); // ToDo: Extend to multiple cameras
        printTimestamp(nodemap, device->getID());
        printPtpConfig(nodemap, device->getID());
        if (!waitForPTPStatus({device}, true, 10)) // ToDo set right value for timeout and change deprecated implementation
        {
            return false;
        }
        auto streams = device->getStreams();
        if (streams.empty())
        {
            std::cerr << "No streams found!" << std::endl;
            return false;
        }

        stream = streams[0];
        stream->open();
        stream->startStreaming();

        enableChunkData(nodemap);      // Enable metadata extraction
        std::cout << "\033[1;32mStreaming started. Press Ctrl+C to stop.\033[0m" << std::endl;
        while (!stop_streaming)
        {
            const rcg::Buffer *buffer = stream->grab(1000);
            if (buffer)
            {
                displayImage(buffer); // Display using OpenCV
            }
        }

        stopStreaming(device);
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return false;
    }
}

// Function to capture a single image
void captureSingleImage(const std::string &format, std::shared_ptr<rcg::Device> device)
{
    try
    {
        std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();
        adjustCameraSettings(nodemap);
        configureSyncFreeRun(nodemap); // Ensure it's in Sync Free Run mode
        enablePTP(nodemap); // ToDo: Extend to multiple cameras
        printTimestamp(nodemap, device->getID());
        printPtpConfig(nodemap, device->getID());
        auto streams = device->getStreams();
        if (streams.empty())
        {
            std::cerr << "No streams found!" << std::endl;
            return;
        }

        stream = streams[0];
        stream->open();
        stream->startStreaming();

        enableChunkData(nodemap);
        std::cout << "Capturing a single image..." << std::endl;

        const rcg::Buffer *buffer = stream->grab(2000);
        if (buffer)
        {
            extractMetadata(nodemap, buffer);
            storeImage(buffer, format);
        }
        else
        {
            std::cerr << "Failed to capture image!" << std::endl;
        }

        stopStreaming(device);
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
    }
}

// Main function to handle both continuous and single-shot modes
int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler); // Handle Ctrl+C to stop streaming
    std::vector<std::shared_ptr<rcg::System>> systems = rcg::System::getSystems();
    int deviceCount = 0;
    for (auto &system : systems)
    {
        system->open();
        for (auto &interf : system->getInterfaces())
        {
            interf->open();
            for (auto &device : interf->getDevices())
            {
                deviceCount++;
                device->open(rcg::Device::CONTROL);
                if (argc > 1 && std::string(argv[1]) == "--single-shot")
                {
                    captureSingleImage("png", device);
                    return 0;
                }

                if (!startStreaming(device))
                {
                    return 1;
                }

                stopStreaming(device);
                device->close();
            }
            interf->close();
        }
        system->close();
    }

    cv::destroyAllWindows(); // Close OpenCV window
    return 0;
}
