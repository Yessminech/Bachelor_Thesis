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


void captureSingleImage(const std::string &format, std::shared_ptr<rcg::Device> device)
{
    try
    {
        std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();
        auto streams = device->getStreams();
        if (streams.empty())
        {
            std::cerr << "No streams found!" << std::endl;
            return;
        }

        stream = streams[0];
        stream->open();
        stream->startStreaming();

        std::cout << "Capturing a single image..." << std::endl;

        const rcg::Buffer *buffer = stream->grab(2000);
        if (buffer)
        {
            storeImage(buffer, format);
        }
        else
        {
            std::cerr << "Failed to capture image!" << std::endl;
        }

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
            throw std::invalid_argument("No arguments provided, streaming on first device found. Use -d followed by the device ID if you want to start the stream on a specific device.");
        }

        std::vector<std::shared_ptr<rcg::System>> systems = rcg::System::getSystems();
        int deviceCount = 0;
        std::string targetDeviceID = (argc > 1) ? argv[1] : "";

        for (auto &system : systems)
        {
            system->open();
            for (auto &interf : system->getInterfaces())
            {
                interf->open();

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
                    auto nodemap = device->getRemoteNodeMap();
                    captureSingleImage("png", device);
                    device->close();
                }
                else
                {
                    for (auto &device : interf->getDevices())
                    {
                        deviceCount++;
                        std::cout << GREEN << "Opening device '" << device->getID() << "'..." << RESET << std::endl;
                        device->open(rcg::Device::CONTROL);
                        auto nodemap = device->getRemoteNodeMap();
                        captureSingleImage("png", device);
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
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}

// ./single_shot 210200799
/*****************/
/* ToDo */
// Multi-threading for parallel streaming 
// Bendwidth configuration
// Grapping Timetsamps from metadata, log and compare them