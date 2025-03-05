#include "StreamManager.hpp"
#include "DeviceManager.hpp"
#include "Camera.hpp"
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

StreamManager::StreamManager()
{
    // Constructor implementation
    stop_streaming = false;
    startedThreads = 0;
    // threads.clear();
    // globalFrames.clear();
    // globalDevices.clear();
    // globalFrameMutex = std::mutex();
}

StreamManager::~StreamManager()
{
    // Destructor implementation
    stop_streaming = true;
    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }
    cv::destroyAllWindows();
    rcg::System::clearSystems();
}

// Signal handler: when Ctrl+C is pressed.
void StreamManager::signalHandler(int signum)
{
    std::cout << "\n"
              << YELLOW << "Stopping streaming..." << RESET << std::endl;
    stop_streaming = true;
}

/// Arrange frames into a composite grid. Supports up to 6 cameras.
/// The grid layout is determined by the number of streams.
cv::Mat StreamManager::createComposite(const std::vector<cv::Mat> &frames)
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

/// Streams from a specific device identified by deviceID.
void StreamManager::streamFromDevice(const std::string &deviceID)
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
            std::cerr << RED << "[Camera::debug] Failed to open system: " << ex.what() << RESET << std::endl;
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
                std::cerr << RED << "[Camera::debug] Failed to open interface: " << ex.what() << RESET << std::endl;
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
            // globalDevices.push_back(device); // ToDo create DeviceManager
            {
                std::lock_guard<std::mutex> lock(globalFrameMutex);
                globalFrames.push_back(cv::Mat());
            }
            int index = static_cast<int>(globalFrames.size()) - 1;
            // if (Camera::debug) // ToDo create Camera object
                std::cout << "[Camera::debug] Launching stream thread for camera " << device->getID() << std::endl;
            // threads.emplace_back(startStreaming, device, index); // ToDo create cam object
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
                std::cerr << RED << "[Camera::debug] Failed to open system: " << ex.what() << RESET << std::endl;
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
                    std::cerr << RED << "[Camera::debug] Failed to open interface: " << ex.what() << RESET << std::endl;
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
                //globalDevices.push_back(device); // ToDo Create DeviceManager
                {
                    std::lock_guard<std::mutex> lock(globalFrameMutex);
                    globalFrames.push_back(cv::Mat());
                }
                int index = static_cast<int>(globalFrames.size()) - 1;
                // if (Camera::debug) // ToDo create Camera object
                std::cout << "[Camera::debug] Launching stream thread for camera " << device->getID() << std::endl;
                // threads.emplace_back(startStreaming, device, index);  // ToDo Create Camera object
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

// int main(int argc, char **argv) // ToDo change to sync free run
// {
    // std::signal(SIGINT, signalHandler); // Install signal handler for Ctrl+C.
    // try
    // {
    //     if (argc > 1)
    //     {
    //         numCams = argc - 1; // ToDo dynamically
    //         for (int i = 1; i < argc; ++i)
    //         {
    //             streamFromDevice(std::string(argv[i]));
    //         }
    //     }
    //     else
    //     {
    //         // ToDo set numCams
    //         std::cout << GREEN << "No device ID provided. Streaming from all available devices." << RESET << std::endl;
    //         streamFromAllDevices();
    //     }

    //     // Wait for all threads to start successfully.
    //     while (startedThreads.load() < globalDevices.size())
    //     {
    //         std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //     }
    //     std::cout << GREEN << "All threads have started successfully." << RESET << std::endl;

    //     // Create a composite window that scales nicely.
    //     const std::string compWindowName = "Composite Stream";
    //     cv::namedWindow(compWindowName, cv::WINDOW_AUTOSIZE);
    //     const int compWidth = 1280, compHeight = 720;
    //     cv::resizeWindow(compWindowName, compWidth, compHeight);

    //     // Composite display loop.
    //     while (!stop_streaming)
    //     {
    //         std::vector<cv::Mat> framesCopy;
    //         {
    //             std::lock_guard<std::mutex> lock(globalFrameMutex);
    //             framesCopy = globalFrames;
    //         }
    //         if (!framesCopy.empty())
    //         {
    //             cv::Mat composite = createComposite(framesCopy);
    //             cv::imshow(compWindowName, composite);
    //         }
    //         int key = cv::waitKey(30);
    //         if (key == 27)
    //         { // ESC key
    //             stop_streaming = true;
    //         }
    //     }

    //     // Wait for all camera threads to finish.
    //     for (auto &t : threads)
    //     {
    //         if (t.joinable())
    //             t.join();
    //     }
    //     cv::destroyAllWindows();
    //     rcg::System::clearSystems();
    // }
    // catch (const std::exception &ex)
    // {
    //     std::cerr << RED << "Main Exception: " << ex.what() << RESET << std::endl;
    //     return -1;
    // }
    // return 0;
// }

// TODO Add periodic ptp sync

//  make && ./continuous_stream devicemodul00_30_53_37_67_42 devicemodul00_30_53_37_67_41 210200799 devicemodul04_5d_4b_79_71_12
// ToDo buffer chech, default mode amd chunk mode in osny cams