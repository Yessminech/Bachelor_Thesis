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

#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"

// Global debug flag (set to true for verbose output).
bool debug = true;
double numCams; // Todo Correct

// Global atomic flag to signal all threads to stop.
std::atomic<bool> stop_streaming(false);

// Global atomic counter to indicate how many threads have successfully started.
std::atomic<int> startedThreads(0);

// Global containers for devices and the latest frame for each device.
std::vector<std::shared_ptr<rcg::Device>> globalDevices;
std::vector<cv::Mat> globalFrames;
std::mutex globalFrameMutex;
std::vector<std::thread> threads;

// Signal handler: when Ctrl+C is pressed.
void signalHandler(int signum)
{
    std::cout << "\n"
              << YELLOW << "Stopping streaming..." << RESET << std::endl;
    stop_streaming = true;
}

// Calculate packet delay (not used in composite display but provided for completeness).
double CalculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams)
{
    double buffer = (bufferPercent / 100.0) * (packetSizeB * (1e9 / deviceLinkSpeedBps));
    return (((packetSizeB * (1e9 / deviceLinkSpeedBps)) + buffer) * (numCams - 1));
}

// Calculate packet delay (not used in composite display but provided for completeness).
double CalculateTransmissionDelayNs(double packetDelayNs, int camIndex)
{
    return packetDelayNs * camIndex;
}

// Set bandwidth parameters on the device.
void setBandwidth(const std::shared_ptr<rcg::Device> &device, double camIndex)
{
    double deviceLinkSpeedBps = 125000000; // 1 Gbps // ToDo What does this mean and how to define this
    double packetSizeB = 8228;             // ToDo What does this mean and how to define this, jumbo frames?
    double bufferPercent = 15;             // 10.93;

    try
    {
        device->open(rcg::Device::CONTROL);
        auto nodemap = device->getRemoteNodeMap();
        double packetDelay = CalculatePacketDelayNs(packetSizeB, deviceLinkSpeedBps, bufferPercent, numCams);
        if (debug)
            std::cout << "[DEBUG] numCams and CamIndex: " << numCams << " " << camIndex << std::endl;
        std::cout << "[DEBUG] Camera " << device->getID() << ": Calculated packet delay: " << packetDelay << " ns" << std::endl;
        double transmissionDelay = CalculateTransmissionDelayNs(packetDelay, camIndex);
        if (debug)
            std::cout << "[DEBUG] Camera " << device->getID() << ": Calculated transmission delay: " << transmissionDelay << " ns" << std::endl;
        int64_t gevSCPDValue = static_cast<int64_t>(packetDelay);
        double gevSCPDValueMs = gevSCPDValue / 1e6;

        rcg::setInteger(nodemap, "GevSCPD", gevSCPDValue);
        if (debug)
            std::cout << "[DEBUG] Camera " << device->getID() << ": GevSCPD set to: " << rcg::getInteger(nodemap, "GevSCPD") << " ns" << std::endl;
        if (device->getID() == "devicemodul04_5d_4b_79_71_12") // Example: skip specific device.
        {
            rcg::setInteger(nodemap, "GevSCPD", gevSCPDValueMs);
        }
        int64_t gevSCFTDValue = static_cast<int64_t>(transmissionDelay);
        double gevSCFTDValueMs = gevSCFTDValue / 1e6;
        if (debug)
            std::cout << "[DEBUG] Camera " << device->getID() << ": GevSCFTD in seconds: " << gevSCFTDValueMs << " s" << std::endl;
        rcg::setInteger(nodemap, "GevSCFTD", gevSCFTDValue);
        if (device->getID() == "devicemodul04_5d_4b_79_71_12") // Example: skip specific device.
        {
            rcg::setEnum(nodemap, "ImageTransferDelayMode", "On");
            rcg::setFloat(nodemap, "ImageTransferDelayValue", gevSCFTDValueMs);
        }

        if (debug)
            std::cout << "[DEBUG] Camera " << device->getID() << ": GevSCFTD set to: " << rcg::getInteger(nodemap, "GevSCFTD") << " ns" << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to set bandwidth for camera " << device->getID() << ": " << ex.what() << RESET << std::endl;
    }
}

/// Enable chunk mode if supported.
void enableChunkData(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID)
{
    if (nodemap)
    {
        try
        {
            rcg::setBoolean(nodemap, "ChunkModeActive", true);
            rcg::setBoolean(nodemap, "ChunkDataControl", true); // add deprecated check
            rcg::setBoolean(nodemap, "ChunkEnable", true);      // add deprecated check

            if (debug)
                std::cout << "[DEBUG] Camera " << camID << ": Chunk mode enabled" << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Failed to enable chunk mode for camera " << camID << ": " << ex.what() << RESET << std::endl;
        }
    }
}

/// Configure the device for continuous (free-run) acquisition.
void configureSyncFreeRun(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID)
{
    if (nodemap)
    {
        try
        {
            rcg::setEnum(nodemap, "AcquisitionMode", "Continuous");
            rcg::setEnum(nodemap, "TriggerSelector", "FrameStart");
            rcg::setEnum(nodemap, "TriggerMode", "Off");
            rcg::setBoolean(nodemap, "AcquisitionFrameRateEnable", true);
            // rcg::setEnum(nodemap, "AcquisitionFrameRateAuto", "Off");
            double maxFrameRate = 20; // rcg::getFloat(nodemap, "AcquisitionFrameRate");
            try
            {
                rcg::setFloat(nodemap, "AcquisitionFrameRate", maxFrameRate);
            }
            catch (const std::exception &e)
            {
                rcg::setFloat(nodemap, "AcquisitionFrameRateAbs", maxFrameRate);
            }

            if (debug)
                std::cout << "[DEBUG] Camera " << camID << ": Configured free-run mode" << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Failed to configure free-run mode for camera " << camID << ": " << ex.what() << RESET << std::endl;
        }
    }
}

/// Set up camera settings such as exposure, gain, and white balance.
void setupCameraSettings(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID)
{
    if (nodemap)
    {
        try
        {
            // Set exposure time.
            rcg::setFloat(nodemap, "ExposureTimeAbs", 222063); // Example: 20 ms
            rcg::setFloat(nodemap, "ExposureTime", 222063);    // Example: 20 ms

            // rcg::setEnum(nodemap, "ExposureAuto","Continuous");

            if (debug)
                std::cout << "[DEBUG] Camera " << camID << ": Exposure time set to 80 ms" << std::endl;

            // Set gain.
            rcg::setFloat(nodemap, "Gain", 0); // Example: Gain of 10 dB
            if (debug)
                std::cout << "[DEBUG] Camera " << camID << ": Gain set to 20 dB" << std::endl;

            // Set white balance.
            rcg::setBoolean(nodemap, "AutoFunctionROIUseWhiteBalance", true);

            rcg::setEnum(nodemap, "BalanceWhiteAuto", "Continuous"); // Example: Auto white balance
            if (debug)
                std::cout << "[DEBUG] Camera " << camID << ": White balance set to auto" << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Failed to set camera settings for camera " << camID << ": " << ex.what() << RESET << std::endl;
        }
    }
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

/// This thread function grabs images from one camera, converts and resizes them,
/// overlays the device ID, timestamp, and FPS, and updates the shared global frame vector at position 'index'.
/// Once the stream is successfully started, it increments the global atomic counter.
void displayCameraStream(const std::shared_ptr<rcg::Device> &device, int index)
{
    try
    {
        if (debug)
            std::cout << "[DEBUG] Opening device " << device->getID() << std::endl;
        device->open(rcg::Device::CONTROL);
        auto nodemap = device->getRemoteNodeMap();
        configureSyncFreeRun(nodemap, device->getID());
        enableChunkData(nodemap, device->getID());
        setBandwidth(device, index);
        setupCameraSettings(nodemap, device->getID());
        // Get the first available stream.
        std::vector<std::shared_ptr<rcg::Stream>> streamList = device->getStreams();
        if (streamList.empty())
        {
            std::cerr << RED << "No stream available for camera " << device->getID() << RESET << std::endl;
            device->close();
            return;
        }
        auto stream = streamList[0];
        try
        {
            stream->open();
            stream->attachBuffers(true);
            stream->startStreaming();
            if (debug)
                std::cout << "[DEBUG] Stream started for camera " << device->getID() << std::endl;
            // Signal that this thread has started successfully.
            startedThreads++;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Failed to start stream for camera " << device->getID()
                      << ": " << ex.what() << RESET << std::endl;

            return;
        }

        // Variables for FPS calculation.
        auto lastTime = std::chrono::steady_clock::now();
        int frameCount = 0;

        while (!stop_streaming)
        {
            const rcg::Buffer *buffer = nullptr;
            try
            {
                buffer = stream->grab(5000);
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[DEBUG] Exception during buffer grab for camera "
                          << device->getID() << ": " << ex.what() << RESET << std::endl;
                continue;
            }
            if (buffer && !buffer->getIsIncomplete() && buffer->getImagePresent(0))
            {
                rcg::Image image(buffer, 0);
                cv::Mat outputFrame;
                uint64_t format = image.getPixelFormat();
                cv::Mat rawFrame(image.getHeight(), image.getWidth(), CV_8UC1, (void *)image.getPixels());
                processRawFrame(rawFrame, outputFrame, format);
                if (!outputFrame.empty())
                {
                    // Resize to a fixed resolution.
                    cv::Mat resizedFrame;
                    cv::resize(outputFrame, resizedFrame, cv::Size(640, 480));

                    // Update FPS calculation.
                    // ToDo is this correct ?
                    frameCount++;
                    auto currentTime = std::chrono::steady_clock::now();
                    double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count() / 1000.0;
                    double fps = (elapsed > 0) ? frameCount / elapsed : 0.0;
                    if (elapsed >= 1.0)
                    {
                        lastTime = currentTime;
                        frameCount = 0;
                    }

                    // Prepare overlay text.
                    uint64_t timestampNS = buffer->getTimestampNS();
                    double timestampSec = static_cast<double>(timestampNS) / 1e9;
                    std::ostringstream oss;
                    oss << "TS: " << std::fixed << std::setprecision(6) << timestampSec << " s"
                        << " | FPS: " << std::fixed << std::setprecision(2) << fps;
                    std::ostringstream camOss;
                    camOss << "Cam: " << device->getID();

                    // Use cv::getTextSize to compute proper placement so text is inside the image.
                    int baseline = 0;
                    cv::Size textSize = cv::getTextSize(oss.str(), cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
                    int textY = resizedFrame.rows - baseline - 5; // 5-pixel margin above bottom.
                    cv::putText(resizedFrame, oss.str(), cv::Point(10, textY),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);

                    // Add camera ID in a separate line.
                    cv::putText(resizedFrame, camOss.str(), cv::Point(10, textY - textSize.height - 10),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);

                    // Update the global frame for this camera.
                    {
                        std::lock_guard<std::mutex> lock(globalFrameMutex);
                        globalFrames[index] = resizedFrame.clone();
                    }
                }
                else
                {
                    if (debug)
                        std::cerr << "[DEBUG] Empty output frame for camera " << device->getID() << std::endl;
                }
            }
            else
            {
                std::cerr << YELLOW << "Camera " << device->getID() << ": Invalid image grabbed." << RESET << std::endl;
            }
        }

        // Cleanup.
        try
        {
            stream->stopStreaming();
            stream->close();
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Exception during stream cleanup for camera " << device->getID()
                      << ": " << ex.what() << RESET << std::endl;
            return;
        }
        device->close();
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Exception in camera " << device->getID() << " thread: "
                  << ex.what() << RESET << std::endl;
        // Cleanup.
        try
        {
            device->close();
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Exception during stream cleanup for camera " << device->getID()
                      << ": " << ex.what() << RESET << std::endl;
            return;
        }
        return;
    }
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

int main(int argc, char **argv)
{
    std::signal(SIGINT, signalHandler); // Install signal handler for Ctrl+C.
    try
    {
        if (argc > 1)
        {
            numCams = argc - 1; // ToDo dynamically
            for (int i = 1; i < argc; ++i)
            {
                streamFromDevice(std::string(argv[i]));
            }
        }
        else
        {
            // ToDo set numCams
            std::cout << GREEN << "No device ID provided. Streaming from all available devices." << RESET << std::endl;
            streamFromAllDevices();
        }

        // Wait for all threads to start successfully.
        while (startedThreads.load() < globalDevices.size())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << GREEN << "All threads have started successfully." << RESET << std::endl;

        // Create a composite window that scales nicely.
        const std::string compWindowName = "Composite Stream";
        cv::namedWindow(compWindowName, cv::WINDOW_AUTOSIZE);
        const int compWidth = 1280, compHeight = 720;
        cv::resizeWindow(compWindowName, compWidth, compHeight);

        // Composite display loop.
        while (!stop_streaming)
        {
            std::vector<cv::Mat> framesCopy;
            {
                std::lock_guard<std::mutex> lock(globalFrameMutex);
                framesCopy = globalFrames;
            }
            if (!framesCopy.empty())
            {
                cv::Mat composite = createComposite(framesCopy);
                cv::imshow(compWindowName, composite);
            }
            int key = cv::waitKey(30);
            if (key == 27)
            { // ESC key
                stop_streaming = true;
            }
        }

        // Wait for all camera threads to finish.
        for (auto &t : threads)
        {
            if (t.joinable())
                t.join();
        }
        cv::destroyAllWindows();
        rcg::System::clearSystems();
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Main Exception: " << ex.what() << RESET << std::endl;
        return -1;
    }
    return 0;
}
// TODO Add periodic ptp sync

//  make && ./continuous_stream devicemodul00_30_53_37_67_42 devicemodul00_30_53_37_67_41 210200799 devicemodul04_5d_4b_79_71_12
// ToDo buffer chech, default mode amd chunk mode in osny cams