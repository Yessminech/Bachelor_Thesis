#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/stream.h>
#include <rc_genicam_api/buffer.h>
#include <rc_genicam_api/image.h>
#include <rc_genicam_api/image_store.h>
#include <rc_genicam_api/nodemap_out.h>
#include <opencv2/opencv.hpp>

#include <iostream>
#include <atomic>
#include <thread>
#include <signal.h>
#include <iomanip>
#include <memory>
#include <string>
#include <stdexcept>
#include <chrono>

#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"

std::atomic<bool> stop_streaming(false);
std::shared_ptr<rcg::Stream> stream;

void signalHandler(int signum)
{
    std::cout << "\nStopping streaming..." << std::endl;
    stop_streaming = true;
}

void enableChunkData(std::shared_ptr<GenApi::CNodeMapRef> nodemap) // ToDo: What s this doing and do I need it ?
{
    if (nodemap)
    {
        rcg::setBoolean(nodemap, "ChunkModeActive", true);
    }
}

void configureSyncFreeRun(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    if (nodemap)
    {
        // ToDo: Add try and catch Exceptions
        rcg::setEnum(nodemap, "AcquisitionMode", "Continuous");
        rcg::setEnum(nodemap, "TriggerSelector", "FrameStart");
        rcg::setEnum(nodemap, "TriggerMode", "Off");
        rcg::setBoolean(nodemap, "AcquisitionFrameRateEnable", true);
        rcg::setFloat(nodemap, "AcquisitionFrameRate", 30.0);
    }
}

void stopStreaming()
{
    if (stream)
    {
        stream->stopStreaming();
        stream->close();
        std::cout << "Streaming stopped." << std::endl;
    }
    cv::destroyAllWindows();
}

void displayImage(const rcg::Buffer *buffer)
{
    if (!buffer || buffer->getIsIncomplete())
    {
        std::cerr << "Received incomplete buffer, skipping display..." << std::endl;
        return;
    }

    try
    {
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
    catch (const std::exception &ex)
    {
        std::cerr << "Error displaying image: " << ex.what() << std::endl;
    }
}

bool startStreaming(std::shared_ptr<rcg::Device> device)
{
    try
    {
        std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();
        auto streams = device->getStreams();
        if (streams.empty())
        {
            std::cerr << "No streams found!" << std::endl;
            return false;
        }

        stream = streams[0];
        stream->open();
        stream->attachBuffers(true);
        stream->startStreaming();

        std::cout << "\033[1;32mStreaming started. Press Ctrl+C to stop.\033[0m" << std::endl;
        while (!stop_streaming)
        {
            const rcg::Buffer *buffer = stream->grab(1000); // ToDo why 1000
            if (buffer)
            {
                displayImage(buffer);
            }
        }
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return false;
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler); // Handle Ctrl+C to stop streaming
    std::set<std::string> printedSerialNumbers;

    try
    {
        if (argc <= 1)
        {
            throw std::invalid_argument("No arguments provided, streaming on first device found. Use -d followed by the device ID if you want to start the stream on a specific device.");
        }

        std::vector<std::shared_ptr<rcg::System>> systems = rcg::System::getSystems();
        int deviceCount = 0;
        std::string targetDeviceID = (argc > 1) ? argv[1] : "";

        for (auto &system : systems)
        {
            std::string systemVendor = system->getVendor();
            system->open();
            for (auto &interf : system->getInterfaces())
            {
                interf->open();

                if (!targetDeviceID.empty())
                {
                    auto device = interf->getDevice(targetDeviceID.c_str());
                    if (!device)
                    {
                        continue;
                    }
                    std::string deviceVendor = device->getVendor();
                    std::string serialNumber = device->getSerialNumber();
                    if (deviceVendor == systemVendor)
                    {
                        printedSerialNumbers.insert(serialNumber);
                        std::cout << GREEN << "Opening device '" << device->getID() << "'..." << RESET << std::endl;
                        device->open(rcg::Device::CONTROL);
                        deviceCount++;
                        auto nodemap = device->getRemoteNodeMap();
                        // configureSyncFreeRun(nodemap);
                        enableChunkData(nodemap);
                        if (!startStreaming(device))
                        {
                            stopStreaming();
                            return 1;
                        }

                        device->close();
                    }
                }
                for (auto &device : interf->getDevices())
                {
                    std::string deviceVendor = device->getVendor();
                    if (deviceVendor == systemVendor)
                    {
                        deviceCount++;
                        std::cout << GREEN << "Opening device '" << device->getID() << "'..." << RESET << std::endl;
                        device->open(rcg::Device::CONTROL);
                        auto nodemap = device->getRemoteNodeMap();
                        // configureSyncFreeRun(nodemap);
                        enableChunkData(nodemap);
                        if (!startStreaming(device))
                        {
                            stopStreaming();
                            return 1;
                        }

                        stopStreaming();
                        device->close();
                    }
                }
                interf->close();
            }
            system->close();
        }

        if (deviceCount == 0 && !targetDeviceID.empty())
        {
            const char *baumerCtiPath = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib";
            if (baumerCtiPath == nullptr)
            {
                std::cerr << RED << "Environment variable GENICAM_GENTL64_PATH is not set." << RESET << std::endl;
                return 1;
            }
            rcg::System::setSystemsPath(baumerCtiPath, nullptr);
            std::vector<std::shared_ptr<rcg::System>> baumerSystems = rcg::System::getSystems();
            for (auto &system : baumerSystems)
            {
                system->open();
                for (auto &interf : system->getInterfaces())
                {
                    interf->open();
                    for (auto &device : interf->getDevices())
                    {
                        std::string serialNumber = device->getSerialNumber();
                        if (printedSerialNumbers.find(serialNumber) == printedSerialNumbers.end())
                        {
                            printedSerialNumbers.insert(serialNumber);
                            deviceCount++;
                            std::cout << GREEN << "Opening device '" << device->getID() << "'..." << RESET << std::endl;
                            device->open(rcg::Device::CONTROL);
                            auto nodemap = device->getRemoteNodeMap();
                            // configureSyncFreeRun(nodemap);
                            enableChunkData(nodemap);
                            if (!startStreaming(device))
                            {
                                stopStreaming();
                                return 1;
                            }

                            stopStreaming();
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
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
// ./countinuous_stream 210200799
