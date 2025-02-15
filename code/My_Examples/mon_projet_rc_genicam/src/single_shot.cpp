#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/stream.h>
#include <rc_genicam_api/buffer.h>
#include <rc_genicam_api/image.h>
#include <rc_genicam_api/image_store.h>
#include <rc_genicam_api/nodemap_out.h>
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
    // if (!buffer || buffer->getIsIncomplete())
    // {
        std::cerr << "Received incomplete buffer, skipping display..." << std::endl;
    //     return;
    // }

    // Convert the buffer to an OpenCV Mat
    rcg::Image image(buffer, 0); // Assume first part contains the image

    cv::Mat frame(image.getHeight(), image.getWidth(), CV_8UC1, (void *)image.getPixels());

    // Convert grayscale to BGR for display
    cv::Mat display_frame;
    cv::cvtColor(frame, display_frame, cv::COLOR_BayerBG2BGR);
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
    cv::destroyAllWindows(); // Close OpenCV window
}

// Function to start streaming
bool startStreaming(std::shared_ptr<rcg::Device> device)
{
    try
    {
        std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();
        adjustCameraSettings(nodemap); // Apply exposure and gain settings
        configureSyncFreeRun(nodemap); // Set to Sync Free Run mode
        // enablePTP(nodemap); // ToDo: Extend to multiple cameras
        // printTimestamp(nodemap, device->getID());
        // printPtpConfig(nodemap, device->getID());
        // if (!waitForPTPStatus({device}, true, 10)) // ToDo set right value for timeout and change deprecated implementation
        // {
        //     return false;
        // }
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
            // if (buffer)
            // {
                displayImage(buffer); // Display using OpenCV
            // }
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
        // enablePTP(nodemap); // ToDo: Extend to multiple cameras
        // printTimestamp(nodemap, device->getID());
        // printPtpConfig(nodemap, device->getID());
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


int main(int argc, char *argv[])
{
    try
    {
        if (argc <= 1)
        {
            throw std::invalid_argument("No arguments provided. Use '--single-shot' or '--stream' followed by the device ID.");
        }

        signal(SIGINT, signalHandler); // Handle Ctrl+C to stop streaming
        std::vector<std::shared_ptr<rcg::System>> systems = rcg::System::getSystems();
        int deviceCount = 0;
        std::string targetDeviceID = (argc > 2) ? argv[2] : "";

        for (auto &system : systems)
        {
            system->open();
            for (auto &interf : system->getInterfaces())
            {
                interf->open();
                // Check if a target device ID is provided
                if (!targetDeviceID.empty())
                {
                    auto device = interf->getDevice(targetDeviceID.c_str());
                    if (!device)
                    {
                        std::cerr << RED << "Device with ID '" << targetDeviceID << "' not found." << RESET << std::endl;
                        continue;
                    }
                    std::cout << GREEN << "Opening device '" << device->getID() << "'..." << RESET << std::endl;
                    device->open(rcg::Device::CONTROL);

                    if (std::string(argv[1]) == "--single-shot")
                    {
                        captureSingleImage("png", device);
                        stopStreaming(device);
                        return 0;
                    }
                    if (std::string(argv[1]) == "--stream")
                    {
                        if (!startStreaming(device))
                        {
                            stopStreaming(device);
                            return 1;
                        }
                    }
                    device->close();
                }
                else
                {
                    // ToDo correct this to not iterate over interfaces
                    for (auto &device : interf->getDevices())
                    {
                        deviceCount++;
                        std::cout << GREEN << "Opening device '" << device->getID() << "'..." << RESET << std::endl;
                        device->open(rcg::Device::CONTROL);

                        if (std::string(argv[1]) == "--single-shot")
                        {
                            captureSingleImage("png", device);
                            stopStreaming(device);
                            return 0;
                        }

                        if (!startStreaming(device))
                        {
                            stopStreaming(device);
                            return 1;
                        }

                        stopStreaming(device);
                        device->close();
                    }
                }
                interf->close();
            }
            system->close();
        }

        if (deviceCount == 0)
        {
            std::cerr << RED << "No devices found." << RESET << std::endl;
            return 1;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}