#include "Camera.hpp"
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
#include <set>
#include <arpa/inet.h>
#include <iomanip>
#include <regex>
#include <vector>


Camera::Camera(std::shared_ptr<rcg::Device> device) : device(device)
{
    if (device)
    {
        device = device;
        device->open(rcg::Device::CONTROL);
        nodemap = device->getRemoteNodeMap();
        setCameraConfig();
        deviceConfig.id = device->getID();
        deviceConfig.vendor = device->getVendor();
        deviceConfig.model = device->getModel();
        deviceConfig.tlType = device->getTLType();
        deviceConfig.displayName = device->getDisplayName();
        deviceConfig.accessStatus = device->getAccessStatus();
        deviceConfig.serialNumber = device->getSerialNumber();
        deviceConfig.version = device->getVersion();
        deviceConfig.timestampFrequency = device->getTimestampFrequency();
        deviceConfig.currentIP = getCurrentIP();
        deviceConfig.MAC = getMAC();
        deviceConfig.deprecatedFW = rcg::getString(nodemap, "PtpEnable").empty(); // ToDo do this in a better way 
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
        device->close();
    }
}

void Camera::setCameraConfig()
{
    try
    {
        rcg::setBoolean(nodemap, "ChunkModeActive", true);
        if (!deviceConfig.deprecatedFW)
        {
            rcg::setBoolean(nodemap, "ChunkDataControl", true); 
            rcg::setBoolean(nodemap, "ChunkEnable", true);      
            rcg::setFloat(nodemap, "ExposureTimeAbs", exposure); 

        }
        // rcg::setEnum(nodemap, "ExposureAuto","Continuous");
        rcg::setFloat(nodemap, "ExposureTime", exposure);    // Example: 20 ms
        rcg::setFloat(nodemap, "Gain", gain);                 // Example: Gain of 10 dB
        rcg::setBoolean(nodemap, "AutoFunctionROIUseWhiteBalance", true);
        rcg::setEnum(nodemap, "BalanceWhiteAuto", "Continuous"); // Example: Auto white balance
        if (debug)
        {
            std::cout << GREEN << "setCameraConfig success" << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to set camera settings for camera " << deviceConfig.id << ": " << ex.what() << RESET << std::endl;
    }
}

// ToDo: Implement
void Camera::setActionCommandDeviceConfig(std::shared_ptr<rcg::Device> device, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, const char *triggerSource, uint32_t actionSelector)
{
    try
    {
        auto nodemap = device->getRemoteNodeMap();

        rcg::setString(nodemap, "ActionUnconditionalMode", "On");

        // Set Action Device Key, Group Key, and Group Mask
        rcg::setInteger(nodemap, "ActionDeviceKey", actionDeviceKey);
        rcg::setInteger(nodemap, "ActionGroupKey", groupKey);
        rcg::setInteger(nodemap, "ActionGroupMask", groupMask);
        rcg::setInteger(nodemap, "ActionSelector", actionSelector);

        // Configure Triggering
        rcg::setEnum(nodemap, "TriggerSelector", "FrameStart");
        rcg::setEnum(nodemap, "TriggerMode", "On");
        rcg::setEnum(nodemap, "TriggerSource", triggerSource);
        std::cout << "✅ Camera " << device->getID() << " configured to start acquisition on Action Command.\n";
        std::cout << "✅ Action Device Key: " << rcg::getInteger(nodemap, "ActionDeviceKey") << ", Group Key: " << rcg::getInteger(nodemap, "ActionGroupKey") << ", Group Mask: " << rcg::getInteger(nodemap, "ActionGroupMask") << ", ActionSelector: " << rcg::getInteger(nodemap, "ActionSelector") << std::endl;
        if (debug)
        {
            std::cout << GREEN << "setActionCommandDeviceConfig success" << RESET << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Failed to configure Action Command Device: " << e.what() << std::endl;
    }
}

void Camera::setPtpConfig()
{
    std::string feature = deviceConfig.deprecatedFW ? "GevIEEE1588" : "PtpEnable";

    try
    {
        rcg::setString(nodemap, feature.c_str(), "true", true);
        ptpConfig.enabled = rcg::getBoolean(nodemap, feature.c_str());
        if (debug)
        {
            std::cout << GREEN << "PTP enable success" << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to enable PTP: " << ex.what() << RESET << std::endl;
    }
}

bool isDottedIP(const std::string &ip)
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

void Camera::getPtpConfig()
{
    try
    {
        if (!deviceConfig.deprecatedFW)
        {
            rcg::callCommand(nodemap, "TimestampLatch");
            rcg::callCommand(nodemap, "PtpDataSetLatch");
            ptpConfig.enabled = rcg::getBoolean(nodemap, "PtpEnable");
            ptpConfig.status = rcg::getString(nodemap, "PtpStatus");
            ptpConfig.clockAccuracy = rcg::getString(nodemap, "PtpClockAccuracy");
            ptpConfig.offsetFromMaster = rcg::getInteger(nodemap, "PtpOffsetFromMaster");
        }
        else
        {
            rcg::callCommand(nodemap, "GevIEEE1588");
            ptpConfig.enabled = rcg::getBoolean(nodemap, "GevIEEE1588");
            ptpConfig.status = rcg::getString(nodemap, "GevIEEE1588Status");
            ptpConfig.clockAccuracy = "Unavailable";
            ptpConfig.offsetFromMaster = -1;
        }
        if (debug)
        {
            std::cout << GREEN << "PTP Parameters success " << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get PTP parameters: " << ex.what() << RESET << std::endl;
    }
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

void Camera::getTimestamps()
{
    try
    {
        if (!deviceConfig.deprecatedFW)
        {
            rcg::callCommand(nodemap, "TimestampLatch");
            ptpConfig.timestamp_ns = rcg::getString(nodemap, "TimestampLatchValue");
            ptpConfig.tickFrequency = ptpConfig.enabled ? "1000000000" : "8000000000"; // 1 GHz for PTP, 8 GHz else.
        }
        else
        {
            rcg::callCommand(nodemap, "GevTimestampControlLatch");
            ptpConfig.timestamp_ns = rcg::getString(nodemap, "GevTimestampValue");
            ptpConfig.tickFrequency = rcg::getString(nodemap, "GevTimestampTickFrequency"); 
        }
        ptpConfig.timestamp_s = std::stoull(ptpConfig.timestamp_ns) / std::stoull(ptpConfig.tickFrequency);
        
        if (debug)
        {
            std::cout << GREEN << "Timestamp success" << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get timestamp: " << ex.what() << RESET << std::endl;
    }
}

/// Process a raw frame based on the pixel format.
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

void Camera::startStreaming(bool stop_streaming, std::mutex globalFrameMutex, std::vector<cv::Mat> globalFrames, int index)
{
/// This thread function grabs images from one camera, converts and resizes them,
/// overlays the device ID, timestamp, and FPS, and updates the shared global frame vector at position 'index'.
/// Once the stream is successfully started, it increments the global atomic counter.
    try
    {
        if (debug)
            std::cout << "[debug] Opening device " << device->getID() << std::endl;
        device->open(rcg::Device::CONTROL); // ToDo remove?
        auto nodemap = device->getRemoteNodeMap();
        //Free run Configuration 
        if (nodemap)
        {
            try
            {
                rcg::setEnum(nodemap, "AcquisitionMode", "Continuous");
                rcg::setEnum(nodemap, "TriggerSelector", "FrameStart");
                rcg::setEnum(nodemap, "TriggerMode", "Off");
                rcg::setBoolean(nodemap, "AcquisitionFrameRateEnable", true);
                // rcg::setEnum(nodemap, "AcquisitionFrameRateAuto", "Off");
                double maxFrameRate = 10; // rcg::getFloat(nodemap, "AcquisitionFrameRate");
                try
                {
                    rcg::setFloat(nodemap, "AcquisitionFrameRate", maxFrameRate);
                }
                catch (const std::exception &e)
                {
                    rcg::setFloat(nodemap, "AcquisitionFrameRateAbs", maxFrameRate);
                }
    
                if (debug)
                    std::cout << "[debug] Camera " << deviceConfig.id << ": Configured free-run mode" << std::endl;
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "Failed to configure free-run mode for camera " << deviceConfig.id << ": " << ex.what() << RESET << std::endl;
            }
        }
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
                std::cout << "[debug] Stream started for camera " << device->getID() << std::endl;
            // Signal that this thread has started successfully.
            // ToDo move this somewhere else // startedThreads++;
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
                std::cerr << RED << "[debug] Exception during buffer grab for camera "
                          << device->getID() << ": " << ex.what() << RESET << std::endl;
                continue;
            }
            if (buffer && !buffer->getIsIncomplete() && buffer->getImagePresent(0))
            {
                rcg::Image image(buffer, 0);
                cv::Mat outputFrame;
                uint64_t format = image.getPixelFormat();
                cv::Mat rawFrame(image.getHeight(), image.getWidth(), CV_8UC1, (void *)image.getPixels());
                Camera::processRawFrame(rawFrame, outputFrame, format);
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
                        std::cerr << "[debug] Empty output frame for camera " << device->getID() << std::endl;
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


// Function to stop streaming
void stopStreaming(std::shared_ptr<rcg::Stream> stream)
{
    if (stream)
    {
        stream->stopStreaming();
        stream->close();
        std::cout << "Streaming stopped." << std::endl;
    }
    rcg::System::clearSystems();
}

// // // ToDo: delete -> only for testing
// int main ()
// {
//     std::shared_ptr<rcg::Device> device = rcg::getDevice("file:///home/user/genicam.vin");
//     Camera camera(device);
//     camera.setPtpConfig();
//     camera.getPtpConfig();
//     camera.getTimestamps();
//     return 0;
// }