#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/stream.h>
#include <rc_genicam_api/buffer.h>
#include <rc_genicam_api/image.h>
#include <rc_genicam_api/image_store.h>
#include <rc_genicam_api/nodemap_out.h>
#include <opencv2/opencv.hpp>
#include "rc_genicam_api/pixel_formats.h"

#include <iostream>
#include <atomic>
#include <thread>
#include <csignal>
#include <iomanip>
#include <memory>
#include <string>
#include <stdexcept>
#include <chrono>
#include <sstream>
#include <set>
#include <vector>
#include <mutex>

// Signal handler: when Ctrl+C is pressed.
void signalHandler(int signum)
{
    std::cout << "\n"
              << YELLOW << "Stopping streaming..." << RESET << std::endl;
    stop_streaming = true;
}

/// Arrange frames into a composite grid. Supports up to 6 cameras.
/// The grid layout is determined by the number of streams.
cv::Mat createComposite(const std::vector<cv::Mat> &frames)
{
    int n = frames.size();
    int rows = 1, cols = 1;
    if (n == 1)
    {
        rows = 1;
        cols = 1;
    }
    else if (n == 2)
    {
        rows = 1;
        cols = 2;
    }
    else if (n <= 4)
    {
        rows = 2;
        cols = 2;
    }
    else if (n <= 6)
    {
        rows = 2;
        cols = 3;
    }
    int compWidth = 1280, compHeight = 720;
    int cellWidth = compWidth / cols;
    int cellHeight = compHeight / rows;

    std::vector<cv::Mat> rowImages;
    for (int r = 0; r < rows; r++)
    {
        std::vector<cv::Mat> cellImages;
        for (int c = 0; c < cols; c++)
        {
            int idx = r * cols + c;
            cv::Mat cell;
            if (idx < n && !frames[idx].empty())
            {
                cv::resize(frames[idx], cell, cv::Size(cellWidth, cellHeight));
            }
            else
            {
                cell = cv::Mat::zeros(cellHeight, cellWidth, CV_8UC3);
            }
            cellImages.push_back(cell);
        }
        cv::Mat rowConcat;
        cv::hconcat(cellImages, rowConcat);
        rowImages.push_back(rowConcat);
    }
    cv::Mat composite;
    cv::vconcat(rowImages, composite);
    return composite;
}

/// Enumerate all devices and launch a streaming thread for each.
void streamFromAllDevices()
{
    std::set<std::string> printedSerialNumbers;
    std::vector<std::shared_ptr<rcg::System>> systems;
    try
    {
        systems = rcg::System::getSystems();
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to get systems: " << ex.what() << RESET << std::endl;
        return;
    }
    if (systems.empty())
    {
        std::cerr << RED << "Error: No systems found." << RESET << std::endl;
        return;
    }
    for (auto &system : systems)
    {
        try
        {
            system->open();
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "[DEBUG] Failed to open system: " << ex.what() << RESET << std::endl;
            continue;
        }
        std::vector<std::shared_ptr<rcg::Interface>> interfaces = system->getInterfaces();
        if (interfaces.empty())
            continue;
        for (auto &interf : interfaces)
        {
            try
            {
                interf->open();
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[DEBUG] Failed to open interface: " << ex.what() << RESET << std::endl;
                continue;
            }
            std::vector<std::shared_ptr<rcg::Device>> deviceList = interf->getDevices();
            for (auto &device : deviceList)
            {
                if (device->getID() == "210201103") // Example: skip specific device.
                    continue;
                if (device->getVendor() != system->getVendor())
                    continue;
                std::string serialNumber = device->getSerialNumber();
                if (printedSerialNumbers.find(serialNumber) != printedSerialNumbers.end())
                    continue;
                printedSerialNumbers.insert(serialNumber);
                globalDevices.push_back(device);
                {
                    std::lock_guard<std::mutex> lock(globalFrameMutex);
                    globalFrames.push_back(cv::Mat());
                }
                int index = static_cast<int>(globalFrames.size()) - 1;
                if (debug)
                    std::cout << "[DEBUG] Launching stream thread for camera " << device->getID() << std::endl;
                threads.emplace_back(displayCameraStream, device, index);
            }
            interf->close();
        }
        system->close();
    }
    try
    {
        const char *defaultCtiPath = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib"; // ToDo add other path mvImpact
        if (defaultCtiPath == nullptr)
        {
            std::cerr << RED << "Environment variable GENICAM_GENTL64_PATH is not set." << RESET << std::endl;
            return;
        }
        rcg::System::setSystemsPath(defaultCtiPath, nullptr);
        std::vector<std::shared_ptr<rcg::System>> defaultSystems = rcg::System::getSystems();
        if (defaultSystems.empty())
        {
            std::cerr << RED << "Error: No systems found." << RESET << std::endl;
            return;
        }
        for (auto &system : defaultSystems)
        {
            try
            {
                system->open();
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[DEBUG] Failed to open system: " << ex.what() << RESET << std::endl;
                continue;
            }
            std::vector<std::shared_ptr<rcg::Interface>> interfaces = system->getInterfaces();
            if (interfaces.empty())
                continue;
            for (auto &interf : interfaces)
            {
                try
                {
                    interf->open();
                }
                catch (const std::exception &ex)
                {
                    std::cerr << RED << "[DEBUG] Failed to open interface: " << ex.what() << RESET << std::endl;
                    continue;
                }
                std::vector<std::shared_ptr<rcg::Device>> deviceList = interf->getDevices();
                for (auto &device : deviceList)
                {
                    if (device->getID() == "210201103") // Example: skip specific device.
                        continue;
                    std::string serialNumber = device->getSerialNumber();
                    if (printedSerialNumbers.find(serialNumber) != printedSerialNumbers.end())
                        continue;
                    printedSerialNumbers.insert(serialNumber);
                    globalDevices.push_back(device);
                    {
                        std::lock_guard<std::mutex> lock(globalFrameMutex);
                        globalFrames.push_back(cv::Mat());
                    }
                    int index = static_cast<int>(globalFrames.size()) - 1;
                    if (debug)
                        std::cout << "[DEBUG] Launching stream thread for camera " << device->getID() << std::endl;
                    threads.emplace_back(displayCameraStream, device, index);
                }
                interf->close();
            }
            system->close();
        }
    }
    catch (const std::exception &ex)
    {
        std::cout << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
        return;
    }
}

/// Streams from a specific device identified by deviceID.
void streamFromDevice(const std::string &deviceID)
{
    std::set<std::string> printedSerialNumbers;
    std::vector<std::shared_ptr<rcg::System>> systems;
    try
    {
        systems = rcg::System::getSystems();
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to get systems: " << ex.what() << RESET << std::endl;
        return;
    }
    if (systems.empty())
    {
        std::cerr << RED << "Error: No systems found." << RESET << std::endl;
        return;
    }
    for (auto &system : systems)
    {
        try
        {
            system->open();
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "[DEBUG] Failed to open system: " << ex.what() << RESET << std::endl;
            continue;
        }
        std::vector<std::shared_ptr<rcg::Interface>> interfaces = system->getInterfaces();
        if (interfaces.empty())
            continue;
        for (auto &interf : interfaces)
        {
            try
            {
                interf->open();
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[DEBUG] Failed to open interface: " << ex.what() << RESET << std::endl;
                continue;
            }
            auto device = interf->getDevice(deviceID.c_str());
            if (!device)
            {
                interf->close();
                continue;
            }
            if (device->getVendor() != system->getVendor())
            {
                interf->close();
                continue;
            }
            std::string serialNumber = device->getSerialNumber();
            if (printedSerialNumbers.find(serialNumber) != printedSerialNumbers.end())
            {
                interf->close();
                continue;
            }
            printedSerialNumbers.insert(serialNumber);
            globalDevices.push_back(device);
            {
                std::lock_guard<std::mutex> lock(globalFrameMutex);
                globalFrames.push_back(cv::Mat());
            }
            int index = static_cast<int>(globalFrames.size()) - 1;
            if (debug)
                std::cout << "[DEBUG] Launching stream thread for camera " << device->getID() << std::endl;
            threads.emplace_back(displayCameraStream, device, index);
            interf->close();
        }
        system->close();
    }
    try
    {
        const char *defaultCtiPath = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib"; // ToDo add other path mvImpact
        if (defaultCtiPath == nullptr)
        {
            std::cerr << RED << "Environment variable GENICAM_GENTL64_PATH is not set." << RESET << std::endl;
            return;
        }
        rcg::System::setSystemsPath(defaultCtiPath, nullptr);
        std::vector<std::shared_ptr<rcg::System>> defaultSystems = rcg::System::getSystems();
        if (defaultSystems.empty())
        {
            std::cerr << RED << "Error: No systems found." << RESET << std::endl;
            return;
        }
        for (auto &system : defaultSystems)
        {
            try
            {
                system->open();
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[DEBUG] Failed to open system: " << ex.what() << RESET << std::endl;
                continue;
            }
            std::vector<std::shared_ptr<rcg::Interface>> interfaces = system->getInterfaces();
            if (interfaces.empty())
                continue;
            for (auto &interf : interfaces)
            {
                try
                {
                    interf->open();
                }
                catch (const std::exception &ex)
                {
                    std::cerr << RED << "[DEBUG] Failed to open interface: " << ex.what() << RESET << std::endl;
                    continue;
                }
                auto device = interf->getDevice(deviceID.c_str());
                if (!device)
                {
                    interf->close();
                    continue;
                }
                std::string serialNumber = device->getSerialNumber();
                if (printedSerialNumbers.find(serialNumber) != printedSerialNumbers.end())
                {
                    interf->close();
                    continue;
                }
                printedSerialNumbers.insert(serialNumber);
                globalDevices.push_back(device);
                {
                    std::lock_guard<std::mutex> lock(globalFrameMutex);
                    globalFrames.push_back(cv::Mat());
                }
                int index = static_cast<int>(globalFrames.size()) - 1;
                if (debug)
                    std::cout << "[DEBUG] Launching stream thread for camera " << device->getID() << std::endl;
                threads.emplace_back(displayCameraStream, device, index);
                interf->close();
            }
            system->close();
        }
    }
    catch (const std::exception &ex)
    {
        std::cout << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
    }
}

bool startStreaming(std::shared_ptr<rcg::Device> device)
{
    try
    {
        std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();
        enablePTP(nodemap); // ToDo: Extend to multiple cameras

        auto streams = device->getStreams();
        if (streams.empty())
        {
            std::cerr << "No streams found!" << std::endl;
            return false;
        }

        stream = streams[0];
        stream->open();
        stream->startStreaming();

        std::cout << "\033[1;32mStreaming started. Press Ctrl+C to stop.\033[0m" << std::endl;
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return false;
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

/// Process a raw frame based on the pixel format.
void processRawFrame(const cv::Mat &rawFrame, cv::Mat &outputFrame, uint64_t pixelFormat)
{
    try
    {
        switch (pixelFormat)
        {
        case BGR8:
            outputFrame = rawFrame.clone();
            break;
        case RGB8:
            cv::cvtColor(rawFrame, outputFrame, cv::COLOR_RGB2BGR);
            break;
        case RGBa8:
            cv::cvtColor(rawFrame, outputFrame, cv::COLOR_RGBA2BGR);
            break;
        case BGRa8:
            cv::cvtColor(rawFrame, outputFrame, cv::COLOR_BGRA2BGR);
            break;
        case Mono8:
            outputFrame = rawFrame.clone();
            break;
        case Mono16:
            rawFrame.convertTo(outputFrame, CV_8UC1, 1.0 / 256.0);
            break;
        case BayerRG8:
            cv::cvtColor(rawFrame, outputFrame, cv::COLOR_BayerBG2BGR); // ToDo
            break;
        case BayerBG8:
            cv::cvtColor(rawFrame, outputFrame, cv::COLOR_BayerBG2BGR);
            break;
        case BayerGB8:
            cv::cvtColor(rawFrame, outputFrame, cv::COLOR_BayerGB2BGR);
            break;
        case BayerGR8:
            cv::cvtColor(rawFrame, outputFrame, cv::COLOR_BayerGR2BGR);
            break;
        case YCbCr422_8:
            cv::cvtColor(rawFrame, outputFrame, cv::COLOR_YUV2BGR_YUY2);
            break;
        case YUV422_8:
            cv::cvtColor(rawFrame, outputFrame, cv::COLOR_YUV2BGR_UYVY);
            break;
        default:
            std::cerr << RED << "Unsupported pixel format: " << pixelFormat << RESET << std::endl;
            outputFrame = rawFrame.clone();
            break;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Exception in processRawFrame: " << ex.what() << RESET << std::endl;
    }
}

// TODO Add periodic ptp sync

//  make && ./continuous_stream devicemodul00_30_53_37_67_42 devicemodul00_30_53_37_67_41 210200799 devicemodul04_5d_4b_79_71_12
// ToDo buffer chech, default mode amd chunk mode in osny cams