/**
 * @brief Library component of the TU Berlin industrial automation framework
 *
 * Copyright (c) 2025
 * TU Berlin, Institut für Werkzeugmaschinen und Fabrikbetrieb
 * Fachgebiet Industrielle Automatisierungstechnik
 * Author: Yessmine Chabchoub
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without specific prior written permission.
 *
 * @note
 * DISCLAIMER
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Camera.hpp"
#include "GlobalSettings.hpp"

#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/stream.h>
#include <rc_genicam_api/image.h>
#include "rc_genicam_api/pixel_formats.h"
#include <rc_genicam_api/nodemap_out.h>

#include <opencv2/opencv.hpp>
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include <arpa/inet.h>
#include <iomanip>
#include <regex>
#include <vector>
#include <atomic>
#include <thread>
#include <sys/stat.h>
#include <chrono>
#include <unordered_map>
#include <stdexcept>
#include <ctime>
#include <numeric>
#include <sys/stat.h> // for mkdir
#include <sys/types.h>
#include <filesystem>

Camera::Camera(std::shared_ptr<rcg::Device> device, CameraConfig cameraConfig, NetworkConfig networkConfig)
    : device(device), cameraConfig(cameraConfig), networkConfig(networkConfig)
{
    if (device)
    {
        this->device = device;
        try
        {
            device->open(rcg::Device::CONTROL);
            if (debug)
                std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": Opened successfully" << RESET << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Error: Failed to open camera " << device->getID() << ": " << ex.what() << RESET << std::endl;
            std::exit(EXIT_FAILURE);
        }

        this->nodemap = device->getRemoteNodeMap();

        // Assign device info
        deviceInfos.id = device->getID();
        deviceInfos.vendor = device->getVendor();
        deviceInfos.model = device->getModel();
        deviceInfos.tlType = device->getTLType();
        deviceInfos.displayName = device->getDisplayName();
        deviceInfos.accessStatus = device->getAccessStatus();
        deviceInfos.serialNumber = device->getSerialNumber();
        deviceInfos.version = device->getVersion();
        deviceInfos.timestampFrequency = device->getTimestampFrequency();
        deviceInfos.currentIP = getCurrentIP();
        deviceInfos.MAC = getMAC();
        deviceInfos.deprecatedFW = rcg::getString(nodemap, "PtpEnable").empty();

        // Populate CameraConfig struct
        if (cameraConfig.exposure > 0 || cameraConfig.gain > 0 || cameraConfig.width > 0 || cameraConfig.height > 0)
        {
            this->cameraConfig = cameraConfig; // Use provided values
        }
        else
        {
            // Read from camera if not provided
            if (deviceInfos.deprecatedFW)
            {
                this->cameraConfig.exposure = rcg::getInteger(nodemap, "ExposureTimeAbs");
            }
            else
            {
                this->cameraConfig.exposure = rcg::getInteger(nodemap, "ExposureTime");
            }
            this->cameraConfig.gain = rcg::getFloat(nodemap, "Gain");
            this->cameraConfig.width = rcg::getInteger(nodemap, "Width");
            this->cameraConfig.height = rcg::getInteger(nodemap, "Height");
            std::string formatString = rcg::getEnum(nodemap, "PixelFormat");
            this->cameraConfig.pixelFormat = getPixelFormat(formatString);
        }

        // Populate NetworkConfig struct
        if (networkConfig.deviceLinkSpeedBps > 0 || networkConfig.packetDelayNs > 0 || networkConfig.transmissionDelayNs > 0)
        {
            this->networkConfig = networkConfig; // Use provided values
        }
    }
    else
    {
        std::cerr << "Error: No valid device provided.\n";
    }
}

Camera::~Camera()
{
    if (device)
    {
        try
        {
            device->close();
            if (debug)
            {
                std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": Closed successfully" << RESET << std::endl;
            }
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Error: Failed to close camera " << device->getID() << ": " << ex.what() << RESET << std::endl;
        }
    }
}

void Camera::startStreaming(std::atomic<bool> &stopStream, std::mutex &globalFrameMutex, std::vector<cv::Mat> &globalFrames, int index, bool saveStream)
{
    auto stream = initializeStream();
    if (!stream)
        return;

    auto lastTime = std::chrono::steady_clock::now();
    int frameCount = 0;
    int consecutiveFailures = 0; // Count failed grabs

    while (!stopStream.load())
    {
        const rcg::Buffer *buffer = nullptr;
        try
        {
            buffer = stream->grab(5000); // Timeout in ms
            if (!buffer)
            {
                consecutiveFailures++;
                if (consecutiveFailures > 10)
                {
                    std::cerr << RED << "Error: Too many failed frame grabs for camera " << device->getID()
                              << ". Stopping stream..." << RESET << std::endl;
                    break; // Exit loop if grabbing fails too often
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Exception during grab: " << ex.what() << RESET << std::endl;
            if (stopStream.load())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        consecutiveFailures = 0;
        cv::Mat outputFrame;
        try
        {
            processFrame(buffer, outputFrame);
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Exception during frame processing: " << ex.what() << RESET << std::endl;
            continue;
        }

        if (!outputFrame.empty())
        {
            updateGlobalFrame(globalFrameMutex, globalFrames, index, outputFrame, frameCount, lastTime);

            if (saveStream && isFpsStableForSaving()) // only save if stable
            {
                try
                {
                    saveFrameToPng(outputFrame);
                }
                catch (const std::exception &ex)
                {
                    std::cerr << RED << "Exception during PNG saving: " << ex.what() << RESET << std::endl;
                }
                lastSavedFps = getLastSavedFps();
            }
            else if (saveStream)
            {
                std::cout << "[INFO] Skipping save — FPS unstable: "
                          << "Current mean FPS = " << getLastSavedFps()
                          << ", Last saved FPS = " << getLastSavedFps() << std::endl;
            }
        }
    }

    // Clean up
    try
    {
        cleanupStream(stream);
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Exception during stream cleanup: " << ex.what() << RESET << std::endl;
    }
}

void Camera::setPtpConfig(bool enable)
{
    std::string feature = deviceInfos.deprecatedFW ? "GevIEEE1588" : "PtpEnable";

    try
    {
        if (enable)
        {
            rcg::setString(nodemap, feature.c_str(), "true", enable);
            ptpConfig.enabled = rcg::getBoolean(nodemap, feature.c_str());
            networkConfig.deviceLinkSpeedBps = rcg::getInteger(nodemap, "DeviceLinkSpeed"); // Bps
            if (deviceInfos.deprecatedFW)
            {
                networkConfig.deviceLinkSpeedBps = rcg::getInteger(nodemap, "GevLinkSpeed") * 1000000; // MBps to Bps
            }
        }
        else
        {
            rcg::setString(nodemap, feature.c_str(), "false", enable);
            ptpConfig.enabled = rcg::getBoolean(nodemap, feature.c_str());
        }
        if (debug)
        {
            std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": PTP " << (enable ? "enabled" : "disabled") << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to set PTP: " << ex.what() << RESET << std::endl;
    }
}

void Camera::setFreeRunMode()
{

    try
    {
        rcg::setEnum(nodemap, "AcquisitionMode", "Continuous");
        rcg::setEnum(nodemap, "TriggerSelector", "FrameStart");
        rcg::setEnum(nodemap, "TriggerMode", "Off");
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to configure free-run mode for camera " << deviceInfos.id << ": " << ex.what() << RESET << std::endl;
    }
}

void Camera::setScheduledActionCommand(uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask)
{
    try
    {
        rcg::setInteger(nodemap, "ActionSelector", 0);
        if (this->deviceInfos.vendor == "Lucid")
        {
            rcg::setEnum(nodemap, "ActionUnconditionalMode", "On");
            rcg::setEnum(nodemap, "TransferControlMode", "Uncontrolled");
            rcg::setEnum(nodemap, "TransferOperationMode", "Continuous");
            rcg::callCommand(nodemap, "TransferStop");
        }
        rcg::setInteger(nodemap, "ActionDeviceKey", actionDeviceKey);
        rcg::setInteger(nodemap, "ActionGroupKey", groupKey);
        rcg::setInteger(nodemap, "ActionGroupMask", groupMask);

        rcg::setEnum(nodemap, "TriggerSelector", "FrameStart");
        rcg::setEnum(nodemap, "TriggerMode", "On");
        rcg::setEnum(nodemap, "TriggerSource", "Action0");

        rcg::callCommand(nodemap, "AcquisitionStart");
        if (debug)
        {
            std::cout << GREEN << "setActionCommandDeviceConfig success" << RESET << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << RED << "Error: Failed to set scheduled action command: " << e.what() << RESET << std::endl;
    }
}

void Camera::setPacketSizeB(double packetSizeB)
{
    try
    {
        packetSizeB = std::ceil(packetSizeB / 4.0) * 4; // Ensure packet size is a multiple of 4
        rcg::setInteger(nodemap, "GevSCPSPacketSize", packetSizeB);
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to set GevSCPSPacketSize: " << ex.what() << RESET << std::endl;
    }
}

void Camera::setBandwidthDelays(const std::shared_ptr<Camera> &camera, double camIndex, double numCams, double packetSizeB, double deviceLinkSpeedBps, double bufferPercent)
{
    try
    {

        networkConfig.packetDelayNs = calculatePacketDelayNs(packetSizeB, deviceLinkSpeedBps, bufferPercent, numCams);
        networkConfig.transmissionDelayNs = networkConfig.packetDelayNs * (numCams - 1 - camIndex);
        networkConfig.packetDelayNs = static_cast<int64_t>((networkConfig.packetDelayNs + 7) / 8) * 8; // Ensure multiple of 8
        rcg::setInteger(camera->nodemap, "GevSCPD", networkConfig.packetDelayNs);                      // in counter units, 1ns resolution if ptp is enabled
        networkConfig.transmissionDelayNs = static_cast<int64_t>((networkConfig.transmissionDelayNs + 7) / 8) * 8;
        rcg::setInteger(camera->nodemap, "GevSCFTD", networkConfig.transmissionDelayNs); // in counter units, 1ns resolution if ptp is enabled

        if (debug)
            std::cout << YELLOW << "[DEBUG] Camera " << device->getID() << " numCams: " << numCams << " | CamIndex: " << camIndex
                      << " | packetSizeB: " << packetSizeB << " | deviceLinkSpeedBps: " << deviceLinkSpeedBps
                      << " | bufferPercent: " << bufferPercent << std::endl;
        std::cout << YELLOW << "[DEBUG] Camera " << device->getID() << ": Calculated packet delay: " << networkConfig.packetDelayNs << " ns, Calculated transmission delay: " << networkConfig.transmissionDelayNs << " ns" << RESET << std::endl;
        std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": GevSCPD set to: " << rcg::getInteger(nodemap, "GevSCPD") << " ns, GevSCFTD set to: " << rcg::getInteger(nodemap, "GevSCFTD") << " ns" << RESET << std::endl;
        logBandwidthDelaysToCSV(device->getID(), networkConfig.packetDelayNs, networkConfig.transmissionDelayNs);
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to set bandwidth for camera " << device->getID() << ": " << ex.what() << RESET << std::endl;
    }
}

void Camera::setFps(double fps)
{
    try
    {
        rcg::setBoolean(nodemap, "AcquisitionFrameRateEnable", true);
        if (!deviceInfos.deprecatedFW)
        {
            rcg::setFloat(nodemap, "AcquisitionFrameRate", fps);
            if (debug)
            {
                std::cout << GREEN << "[DEBUG] Camera " << deviceInfos.id << " fps set to:" << rcg::getFloat(nodemap, "AcquisitionFrameRate") << RESET << std::endl;
            }
        }
        else
        {
            rcg::setFloat(nodemap, "AcquisitionFrameRateAbs", fps);
            if (debug)
                std::cout << GREEN << "[DEBUG] Camera " << deviceInfos.id << " fps set to:" << rcg::getFloat(nodemap, "AcquisitionFrameRateAbs") << RESET << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to set frame rate for camera " << deviceInfos.id << ": " << e.what() << std::endl;
    }
}

void Camera::setGain(float gain)
{
    try
    {

        rcg::setFloat(nodemap, "Gain", gain);

        if (debug)
        {

            std::cout << GREEN << "[DEBUG] Camera " << deviceInfos.id << " Gain set to:" << rcg::getFloat(nodemap, "Gain") << RESET << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to set exposure time for camera " << deviceInfos.id << ": " << e.what() << std::endl;
    }
}

void Camera::setHeight(int height)
{
    try
    {
        rcg::setInteger(nodemap, "Height", height);
        if (debug)
        {
            std::cout << GREEN << "[DEBUG] Camera " << deviceInfos.id << " Height set to:" << rcg::getInteger(nodemap, "Height") << RESET << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to set height for camera " << deviceInfos.id << ": " << e.what() << std::endl;
    }
}

void Camera::setWidth(int width)
{
    try
    {
        rcg::setInteger(nodemap, "Width", width);
        if (debug)
        {
            std::cout << GREEN << "[DEBUG] Camera " << deviceInfos.id << " Width set to:" << rcg::getInteger(nodemap, "Width") << RESET << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to set width for camera " << deviceInfos.id << ": " << e.what() << std::endl;
    }
}

void Camera::setExposureTime(double exposureTime)
{
    try
    {
        rcg::setEnum(nodemap, "ExposureMode", "Timed");
        rcg::setEnum(nodemap, "ExposureAuto", "Off");
        if (deviceInfos.deprecatedFW)
        {
            rcg::setFloat(nodemap, "ExposureTimeAbs", exposureTime);
        }
        else
        {
            rcg::setFloat(nodemap, "ExposureTime", exposureTime);
        }

        if (debug)
            if (deviceInfos.deprecatedFW)
            {
                std::cout << GREEN << "[DEBUG] Camera " << deviceInfos.id << " ExposureTime set to:" << rcg::getFloat(nodemap, "ExposureTimeAbs") << RESET << std::endl;
            }
            else
            {
                std::cout << GREEN << "[DEBUG] Camera " << deviceInfos.id << " ExposureTime set to:" << rcg::getFloat(nodemap, "ExposureTime") << RESET << std::endl;
            }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to set exposure time for camera " << deviceInfos.id << ": " << e.what() << std::endl;
    }
}

void Camera::setPixelFormat(std::string pixelFormat)
{
    try
    {
        rcg::setEnum(nodemap, "PixelFormat", pixelFormat.c_str());
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to set pixel format for camera " << deviceInfos.id << ": " << e.what() << std::endl;
    }
}

void Camera::issueScheduledActionCommandLucid(uint32_t actionDeviceKey, uint32_t actionGroupKey, uint32_t actionGroupMask, int64_t scheduledDelayS, const std::string &targetIP)
{
    try
    {

        rcg::setString(nodemap, "ActionCommandTargetIP", targetIP.c_str());
        rcg::setInteger(nodemap, "ActionCommandExecuteTime", scheduledDelayS);

        rcg::callCommand(nodemap, "ActionCommandFireCommand");

        if (debug)
        {
            std::cout << GREEN << "issueScheduledActionCommand success" << RESET << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << RED << "Error: Failed to issue scheduled action command: " << e.what() << RESET << std::endl;
    }
}

double Camera::calculateFps(double deviceLinkSpeedBps, double packetSizeB)
{
    double frameTransmissionCycle = calculateFrameTransmissionCycle(deviceLinkSpeedBps, packetSizeB);
    return std::floor(1 / frameTransmissionCycle);
}

double Camera::getExposureTime()
{
    double exposure = 0.0;
    try
    {
        if (deviceInfos.deprecatedFW)
        {
            exposure = rcg::getFloat(nodemap, "ExposureTimeAbs");
        }
        else
        {
            exposure = rcg::getFloat(nodemap, "ExposureTime");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to set exposure time for camera " << deviceInfos.id << ": " << e.what() << std::endl;
    }
    return exposure;
}

double Camera::getFps()
{
    double fps = 0.0;
    try
    {
        if (!deviceInfos.deprecatedFW)
        {
            fps = rcg::getFloat(nodemap, "AcquisitionFrameRate");
        }
        else
        {
            fps = rcg::getFloat(nodemap, "AcquisitionFrameRateAbs");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to get frame rate for camera " << deviceInfos.id << ": " << e.what() << std::endl;
    }
    return fps;
}

std::string Camera::getCurrentIP()
{
    try
    {
        std::string currentIP = rcg::getString(nodemap, "GevCurrentIPAddress");
        if (!isDottedIP(currentIP))
        {
            try
            {
                if (currentIP.find_first_not_of("0123456789") == std::string::npos)
                {
                    currentIP = decimalToIP(std::stoul(currentIP));
                }
                else
                {
                    currentIP = hexToIP(currentIP);
                }
            }
            catch (const std::invalid_argument &e)
            {
                std::cerr << "Invalid IP address format: " << currentIP << std::endl;
                currentIP = "";
            }
        }
        return currentIP;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return "";
    }
}
std::string Camera::getMAC()
{
    std::regex macPattern(R"(^([0-9A-Fa-f]{2}:){5}([0-9A-Fa-f]{2})$)");
    try
    {
        std::string MAC = rcg::getString(nodemap, "GevMACAddress");
        if (!std::regex_match(MAC, macPattern))
        {
            try
            {
                if (MAC.find_first_not_of("0123456789") == std::string::npos)
                {
                    uint64_t decimalMAC = std::stoull(MAC);
                    MAC = decimalToMAC(decimalMAC);
                }
                else
                {
                    MAC = hexToMAC(MAC);
                }
            }
            catch (const std::invalid_argument &e)
            {
                std::cerr << "Invalid MAC address format: " << MAC << std::endl;
                MAC = "";
            }
        }
        return MAC;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return "";
    }
}

void Camera::getPtpConfig()
{
    try
    {
        if (!deviceInfos.deprecatedFW)
        {
            rcg::callCommand(nodemap, "PtpDataSetLatch");
            ptpConfig.enabled = rcg::getBoolean(nodemap, "PtpEnable");
            ptpConfig.status = rcg::getString(nodemap, "PtpStatus");
            ptpConfig.offsetFromMaster = rcg::getInteger(nodemap, "PtpOffsetFromMaster");
        }
        else
        {
            rcg::callCommand(nodemap, "GevIEEE1588DataSetLatch");
            ptpConfig.enabled = rcg::getBoolean(nodemap, "GevIEEE1588");
            ptpConfig.status = rcg::getString(nodemap, "GevIEEE1588Status");
            try
            {
                // Basler Cameras
                ptpConfig.offsetFromMaster = rcg::getInteger(nodemap, "GevIEEE1588OffsetFromMaster");
            }
            catch (const std::exception &ex)
            {
                ptpConfig.offsetFromMaster = 0; // Unavailable
            }
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get PTP parameters: " << ex.what() << RESET << std::endl;
    }
}

void Camera::getTimestamps()
{
    try
    {
        if (!deviceInfos.deprecatedFW)
        {
            rcg::callCommand(this->nodemap, "TimestampLatch");
            ptpConfig.timestamp_ns = rcg::getInteger(this->nodemap, "TimestampLatchValue");
        }
        else
        {
            rcg::callCommand(this->nodemap, "GevTimestampControlLatch");
            ptpConfig.timestamp_ns = rcg::getInteger(this->nodemap, "GevTimestampValue");
        }
        ptpConfig.timestamp_s = static_cast<double>(ptpConfig.timestamp_ns) / 1e9;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get timestamp: " << ex.what() << RESET << std::endl;
    }
}

void Camera::processRawFrame(const cv::Mat &rawFrame, cv::Mat &outputFrame, uint64_t pixelFormat)
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

void Camera::updateGlobalFrame(std::mutex &globalFrameMutex, std::vector<cv::Mat> &globalFrames, int index,
                               cv::Mat &frame, int &frameCount, std::chrono::steady_clock::time_point &lastTime)
{
    static std::deque<double> fpsHistory;
    int maxHistorySize = ptpMaxCheck;

    frameCount++;
    auto currentTime = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count() / 1000.0;

    double fps = (elapsed > 0) ? frameCount / elapsed : 0.0;
    fpsHistory.push_back(fps);
    if (fpsHistory.size() > maxHistorySize)
        fpsHistory.pop_front();

    double sumFps = std::accumulate(fpsHistory.begin(), fpsHistory.end(), 0.0);
    double meanFps = sumFps / fpsHistory.size();

    static double lastFpsAdjustment = getGlobalFpsUpperBound();
    adjustFpsDynamically(meanFps, lastFpsAdjustment);

    if (elapsed >= 1.0)
    {
        lastTime = currentTime;
        frameCount = 0;
    }

    this->getTimestamps();
    uint64_t timestampNS = ptpConfig.timestamp_ns;

    std::ostringstream overlayText;
    overlayText << "FPS: " << std::fixed << std::setprecision(2) << meanFps;

    std::ostringstream camText;
    camText << "Cam: " << device->getID();

    int baseline = 0;
    cv::Size textSize = cv::getTextSize(overlayText.str(), cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
    int textY = frame.rows - baseline - 5;
    cv::putText(frame, overlayText.str(), cv::Point(10, textY), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
    cv::putText(frame, camText.str(), cv::Point(10, textY - textSize.height - 10), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);

    {
        std::lock_guard<std::mutex> lock(globalFrameMutex);
        globalFrames[index] = frame.clone();
    }
    lastSavedFps = meanFps;
}

void Camera::processFrame(const rcg::Buffer *buffer, cv::Mat &outputFrame)
{
    if (buffer && !buffer->getIsIncomplete() && buffer->getImagePresent(0))
    {
        rcg::Image image(buffer, 0);
        cv::Mat rawFrame(image.getHeight(), image.getWidth(), CV_8UC1, (void *)image.getPixels());
        uint64_t format = image.getPixelFormat();

        Camera::processRawFrame(rawFrame, outputFrame, format);

        // Resize the frame to a fixed resolution
        if (!outputFrame.empty())
        {
            cv::resize(outputFrame, outputFrame, cv::Size(640, 480));
        }
    }
    else
    {
        std::cerr << YELLOW << "Camera " << device->getID() << ": Invalid image grabbed." << RESET << std::endl;
        return;
    }
}

void Camera::cleanupStream(std::shared_ptr<rcg::Stream> &stream)
{
    try
    {
        if (stream)
        {
            stream->stopStreaming();
            stream->close();
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Exception during stream cleanup for camera " << device->getID()
                  << ": " << ex.what() << RESET << std::endl;
    }
    rcg::System::clearSystems();
}

std::shared_ptr<rcg::Stream> Camera::initializeStream()
{
    auto streamList = device->getStreams();
    if (streamList.empty())
    {
        std::cerr << RED << "No stream available for camera " << device->getID() << RESET << std::endl;
        return nullptr;
    }

    auto stream = streamList[0]; // Select the first available stream
    try
    {
        stream->open();
        stream->attachBuffers(true);
        stream->startStreaming();
        if (debug)
            std::cout << GREEN << "[DEBUG] Stream started for camera " << device->getID() << RESET << std::endl;

        return stream;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to start stream for camera " << device->getID() << ": " << ex.what() << RESET << std::endl;
        return nullptr;
    }
}

void Camera::saveFrameToPng(const cv::Mat &frame)
{
    if (frame.empty())
    {
        std::cerr << RED << "[ERROR] Cannot save empty frame to PNG." << RESET << std::endl;
        return;
    }

    // Output path construction
    const std::string outputDir = "./output/recordings";
    const std::string sessionTimestamp = getSessionTimestamp(); // e.g. "20250330_113200"
    const std::string sessionDir = outputDir + "/" + sessionTimestamp;
    const std::string camID = deviceInfos.serialNumber;
    const std::string camDir = sessionDir + "/" + camID + "_" + sessionTimestamp;

    try
    {
        std::filesystem::create_directories(camDir);
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "[ERROR] Failed to create output directories: " << ex.what() << RESET << std::endl;
        return;
    }

    // Convert grayscale to BGR if needed
    cv::Mat pngFrame;
    if (frame.channels() == 1)
    {
        cv::cvtColor(frame, pngFrame, cv::COLOR_GRAY2BGR);
    }
    else
    {
        pngFrame = frame;
    }

    // Save PNG with timestamp
    auto now = std::chrono::high_resolution_clock::now();
    auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    const std::string filename = camDir + "/frame_" + std::to_string(nowNs) + ".png";

    try
    {
        if (!cv::imwrite(filename, pngFrame))
        {
            std::cerr << RED << "[ERROR] Failed to write PNG: " << filename << RESET << std::endl;
        }
        else if (debug)
        {
            std::cout << GREEN << "[INFO] Saved PNG: " << filename << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "[ERROR] Exception writing PNG: " << ex.what() << RESET << std::endl;
    }
}

/***********************************************************************/
/***********************         Calculations     *************************/
/***********************************************************************/

double Camera::calculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams) // ToDo for chunkdata
{
    double buffer = (bufferPercent / 100.0) * (packetSizeB * (1e9 / deviceLinkSpeedBps));
    return (((packetSizeB * (1e9 / deviceLinkSpeedBps)) + buffer) * (numCams - 1));
}

double Camera::calculateFrameTransmissionCycle(double deviceLinkSpeedBps, double packetSizeB)
{
    double RawFrameSizeBytes = calculateRawFrameSizeB(cameraConfig.width, cameraConfig.height, cameraConfig.pixelFormat);
    double numPacketPerFrameB = RawFrameSizeBytes / packetSizeB;
    double frameTransmissionCycle = RawFrameSizeBytes / deviceLinkSpeedBps;
    return frameTransmissionCycle + numPacketPerFrameB * networkConfig.packetDelayNs * 1e-9; // ToDo check if correct, should i add transmission delay
}

double Camera::calculateRawFrameSizeB(int width, int height, PfncFormat_ pixelFormat)
{
    int bitsPerPixel = getBitsPerPixel(pixelFormat);
    int totalPixels = width * height;
    int totalBits = totalPixels * bitsPerPixel;

    return static_cast<double>(totalBits) / 8.0; // Convert bits to bytes
}

/*******************************************************************************/
/***********************         Utility Functions     *************************/
/*******************************************************************************/

bool Camera::isFpsStableForSaving(double tolerance) const
{
    return std::abs(getLastSavedFps() - getLastSavedFps()) <= tolerance; // ToDo
}

void Camera::adjustFpsDynamically(double meanFps, double &lastFpsAdjustment)
{
    double currentFps = getFps();
    double globalMaxFps = getGlobalFpsUpperBound();

    if (std::abs(meanFps - lastFpsAdjustment) > 1.0)
    {
        double newFps = lastFpsAdjustment * 0.98;
        setGlobalFpsUpperBound(newFps);
        setFps(newFps);
        lastFpsAdjustment = newFps;

        std::cout << "[DEBUG] FPS dynamically adjusted to: " << newFps << std::endl;
    }
}

bool Camera::isDottedIP(const std::string &ip)
{
    std::regex ipPattern(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
    std::smatch match;

    if (std::regex_match(ip, match, ipPattern))
    {
        for (int i = 1; i <= 4; ++i)
        {
            int octet = std::stoi(match[i].str());
            if (octet < 0 || octet > 255)
                return false;
        }
        return true;
    }
    return false;
}

std::string Camera::decimalToIP(uint32_t decimalIP)
{
    struct in_addr ip_addr;
    ip_addr.s_addr = htonl(3232236391);
    return std::string(inet_ntoa(ip_addr));
}

std::string Camera::hexToIP(const std::string &hexIP)
{
    uint32_t decimalIP = std::stoul(hexIP, nullptr, 16);
    return decimalToIP(decimalIP);
}

std::string Camera::decimalToMAC(uint64_t decimalMAC)
{
    std::ostringstream macStream;
    for (int i = 5; i >= 0; --i)
    {
        macStream << std::hex << std::setw(2) << std::setfill('0') << ((decimalMAC >> (i * 8)) & 0xFF);
        if (i > 0)
            macStream << ":";
    }
    return macStream.str();
}

std::string Camera::hexToMAC(const std::string &hexMAC)
{
    uint64_t decimalMAC = std::stoull(hexMAC, nullptr, 16);
    return decimalToMAC(decimalMAC);
}

PfncFormat_ Camera::getPixelFormat(const std::string &format)
{
    // Map common format strings to enum values
    if (format == "BGR8")
        return BGR8;
    if (format == "RGB8")
        return RGB8;
    if (format == "RGBa8")
        return RGBa8;
    if (format == "BGRa8")
        return BGRa8;
    if (format == "Mono8")
        return Mono8;
    if (format == "Mono16")
        return Mono16;
    if (format == "BayerRG8")
        return BayerRG8;
    if (format == "BayerBG8")
        return BayerBG8;
    if (format == "BayerGB8")
        return BayerGB8;
    if (format == "BayerGR8")
        return BayerGR8;
    if (format == "YCbCr422_8")
        return YCbCr422_8;
    if (format == "YUV422_8")
        return YUV422_8;

    // Default case if no match found
    std::cerr << RED << "Unknown pixel format string: " << format << RESET << std::endl;
    return Mono8; // Return a default format as fallback
}

int Camera::getBitsPerPixel(PfncFormat_ pixelFormat)
{
    switch (pixelFormat)
    {
    // 24-bit formats (3 channels × 8 bits)
    case BGR8:
    case RGB8:
        return 24;

    // 32-bit formats (4 channels × 8 bits)
    case RGBa8:
    case BGRa8:
        return 32;

    // 8-bit formats (1 channel × 8 bits)
    case Mono8:
    case BayerRG8:
    case BayerBG8:
    case BayerGB8:
    case BayerGR8:
        return 8;

    // 16-bit formats (1 channel × 16 bits)
    case Mono16:
        return 16;

    // YUV/YCbCr formats (typically 16 bits per pixel for YUY2/UYVY)
    case YCbCr422_8:
    case YUV422_8:
        return 16; // 4:2:2 subsampling means 16 bits per pixel

    default:
        std::cerr << RED << "Unknown pixel format: " << pixelFormat << RESET << std::endl;
        return 0;
    }
}

void Camera::logBandwidthDelaysToCSV(const std::string &camID, int64_t packetDelayNs, int64_t transmissionDelayNs)
{
    const std::string outputDir = "./output";
    const std::string bandwidthDir = outputDir + "/bandwidth";

    const std::string sessionStartTimeStr = getSessionTimestamp();
    const std::string csvPath = bandwidthDir + "/bandwidth_delays_" + sessionStartTimeStr + ".csv";

    std::filesystem::create_directories(bandwidthDir);

    bool writeHeader = !std::filesystem::exists(csvPath);

    std::ofstream csvFile(csvPath, std::ios::app);
    if (csvFile.is_open())
    {
        if (writeHeader)
            csvFile << "CameraID,PacketDelayNs,TransmissionDelayNs\n";

        csvFile << camID << "," << packetDelayNs << "," << transmissionDelayNs << "\n";
        csvFile.close();
    }
    else
    {
        std::cerr << RED << "[ERROR] Could not open " << csvPath << " for writing." << RESET << std::endl;
    }
}
