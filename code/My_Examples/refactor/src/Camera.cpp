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
        try
        {
            device->open(rcg::Device::CONTROL);
            if (debug)
                std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": Opened successfully" << RESET << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Error: Failed to open camera " << device->getID() << ": " << ex.what() << RESET << std::endl;
        }
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
        rcg::setFloat(nodemap, "ExposureTime", exposure); // Example: 20 ms
        rcg::setFloat(nodemap, "Gain", gain);             // Example: Gain of 10 dB
        rcg::setBoolean(nodemap, "AutoFunctionROIUseWhiteBalance", true);
        rcg::setEnum(nodemap, "BalanceWhiteAuto", "Continuous"); // Example: Auto white balance
        if (debug)
        {
            std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": setCameraConfig success" << RESET << std::endl;
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
            std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": setPtpConfig success" << RESET << std::endl;
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

void Camera::setFreeRunMode()
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
                std::cout << GREEN << "[debug] Camera " << deviceConfig.id << ": setFreeRunMode success" << RESET << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Failed to configure free-run mode for camera " << deviceConfig.id << ": " << ex.what() << RESET << std::endl;
        }
    }
}

void Camera::updateGlobalFrame(std::mutex &globalFrameMutex, std::vector<cv::Mat> &globalFrames, int index,
                               cv::Mat &frame, int &frameCount, std::chrono::steady_clock::time_point &lastTime)
{
    // Update FPS calculation
    frameCount++;
    auto currentTime = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count() / 1000.0;
    double fps = (elapsed > 0) ? frameCount / elapsed : 0.0;

    if (elapsed >= 1.0)
    {
        lastTime = currentTime;
        frameCount = 0;
    }

    // Prepare overlay text
    uint64_t timestampNS = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime.time_since_epoch()).count();
    double timestampSec = static_cast<double>(timestampNS) / 1e9;

    std::ostringstream overlayText;
    overlayText << "TS: " << std::fixed << std::setprecision(6) << timestampSec << " s"
                << " | FPS: " << std::fixed << std::setprecision(2) << fps;

    std::ostringstream camText;
    camText << "Cam: " << device->getID();

    // Overlay text on frame
    int baseline = 0;
    cv::Size textSize = cv::getTextSize(overlayText.str(), cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
    int textY = frame.rows - baseline - 5;
    cv::putText(frame, overlayText.str(), cv::Point(10, textY), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
    cv::putText(frame, camText.str(), cv::Point(10, textY - textSize.height - 10), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);

    // Update global frame
    {
        std::lock_guard<std::mutex> lock(globalFrameMutex);
        globalFrames[index] = frame.clone();
    }
}

void Camera::processFrame(const rcg::Buffer *buffer, cv::Mat &outputFrame)
{
    if (!buffer || buffer->getIsIncomplete() || !buffer->getImagePresent(0))
    {
        std::cerr << YELLOW << "Camera " << device->getID() << ": Invalid image grabbed." << RESET << std::endl;
        return;
    }

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

const rcg::Buffer *Camera::grabFrame(std::shared_ptr<rcg::Stream> &stream)
{
    try
    {
        return stream->grab(5000);
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "[debug] Exception during buffer grab for camera "
                  << device->getID() << ": " << ex.what() << RESET << std::endl;
        return nullptr;
    }
}

void Camera::cleanupStream(std::shared_ptr<rcg::Stream> &stream)
{
    try
    {
        stream->stopStreaming();
        stream->close();
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Exception during stream cleanup for camera " << device->getID()
                  << ": " << ex.what() << RESET << std::endl;
    }
}

std::shared_ptr<rcg::Stream> Camera::initializeStream()
{
    auto streamList = device->getStreams();
    if (streamList.empty())
    {
        std::cerr << RED << "No stream available for camera " << device->getID() << RESET << std::endl;
        device->close();
        return nullptr;
    }

    auto stream = streamList[0];
    try
    {
        stream->open();
        stream->attachBuffers(true);
        stream->startStreaming();
        if (debug)
            std::cout << "[debug] Stream started for camera " << device->getID() << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to start stream for camera " << device->getID()
                  << ": " << ex.what() << RESET << std::endl;
        return nullptr;
    }

    return stream;
}

void Camera::initializeVideoWriter(const std::string &directory, int width, int height)
{
    // Get current timestamp in seconds
    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&now_time_t);

    // Format the timestamp as a string
    std::ostringstream timestampStream;
    timestampStream << std::put_time(&tm, "%Y%m%d_%H%M%S");

    // Generate the output filename using the timestamp
    std::string outputFilename = "camera_footage_" + timestampStream.str() + ".avi";

    // Generate the full output path
    std::string outputPath = directory + "/" + outputFilename;

    // Open the video writer
    videoWriter.open(outputPath, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 30.0, cv::Size(width, height), true);

    if (!videoWriter.isOpened())
    {
        std::cerr << "Error: Unable to open video writer for " << outputPath << std::endl;
    }
    else
    {
        std::cout << "Video recording started at: " << outputPath << std::endl;
        saveFootage = true;
    }
}

void Camera::saveFrameToVideo(cv::VideoWriter &videoWriter, const cv::Mat &frame)
{
    if (videoWriter.isOpened())
    {
        videoWriter.write(frame);
    }
    else
    {
        std::cerr << "Error: Video writer is not opened." << std::endl;
    }
}

void Camera::startStreaming(bool stop_streaming, std::mutex &globalFrameMutex, std::vector<cv::Mat> &globalFrames, int index, bool saveFootage)
{
    // Set camera to free-run mode
    Camera::setFreeRunMode();

    // Initialize and open the camera stream
    auto stream = initializeStream();
    if (!stream)
        return;

    // Optionally initialize video writer if saving footage is enabled
    if (saveFootage)
    {
        initializeVideoWriter("./output", 640, 480); // Set output directory and resolution
    }

    // Variables for FPS calculation
    auto lastTime = std::chrono::steady_clock::now();
    int frameCount = 0;

    while (!stop_streaming)
    {
        // Grab a frame from the stream
        auto buffer = grabFrame(stream);
        if (!buffer)
            continue;

        // Process the frame
        cv::Mat outputFrame;
        processFrame(buffer, outputFrame);

        if (!outputFrame.empty())
        {
            // Update the global frame
            updateGlobalFrame(globalFrameMutex, globalFrames, index, outputFrame, frameCount, lastTime);

            // If saving footage, write the frame to the video file
            if (saveFootage)
            {
                saveFrameToVideo(videoWriter, outputFrame);
            }
        }
        else if (debug)
        {
            std::cerr << "[debug] Empty output frame for camera " << device->getID() << std::endl;
        }
    }

    // Clean up the stream and release the video writer if necessary
    cleanupStream(stream);
    if (saveFootage)
    {
        videoWriter.release();
        std::cout << "Footage saved successfully!" << std::endl;
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

double Camera::CalculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams)
{
    double buffer = (bufferPercent / 100.0) * (packetSizeB * (1e9 / deviceLinkSpeedBps));
    return (((packetSizeB * (1e9 / deviceLinkSpeedBps)) + buffer) * (numCams - 1));
}

double Camera::CalculateTransmissionDelayNs(double packetDelayNs, int camIndex)
{
    return packetDelayNs * camIndex;
}

// Set bandwidth parameters on the device.
void Camera::setBandwidth(const std::shared_ptr<Camera> &camera, double camIndex, double numCams, double packetSizeB, double deviceLinkSpeedBps, double bufferPercent)
{
    try
    {
        double packetDelay = CalculatePacketDelayNs(packetSizeB, deviceLinkSpeedBps, bufferPercent, numCams);
        double transmissionDelay = CalculateTransmissionDelayNs(packetDelay, camIndex);
        int64_t gevSCPDValue = static_cast<int64_t>(packetDelay);
        double gevSCPDValueMs = gevSCPDValue / 1e6;
        rcg::setInteger(camera->nodemap, "GevSCPD", gevSCPDValue);
        int64_t gevSCFTDValue = static_cast<int64_t>(transmissionDelay);
        double gevSCFTDValueMs = gevSCFTDValue / 1e6;
        rcg::setInteger(camera->nodemap, "GevSCFTD", gevSCFTDValue);

        if (debug)
            std::cout << "[DEBUG] numCams and CamIndex: " << numCams << " " << camIndex << std::endl;
        std::cout << "[DEBUG] Camera " << device->getID() << ": Calculated packet delay: " << packetDelay << " ns" << std::endl;
        std::cout << "[DEBUG] Camera " << device->getID() << ": Calculated transmission delay: " << transmissionDelay << " ns" << std::endl;
        std::cout << "[DEBUG] Camera " << device->getID() << ": GevSCPD set to: " << rcg::getInteger(nodemap, "GevSCPD") << " ns" << std::endl;
        std::cout << "[DEBUG] Camera " << device->getID() << ": GevSCFTD in seconds: " << gevSCFTDValueMs << " s" << std::endl;
        std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": setBandwidth sucess" << std::endl;

        // if (device->getID() == "devicemodul04_5d_4b_79_71_12") // Example: skip specific device.
        // {
        //     rcg::setEnum(camera->nodemap, "ImageTransferDelayMode", "On");
        //     rcg::setFloat(camera->nodemap, "ImageTransferDelayValue", gevSCFTDValueMs);
        // }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to set bandwidth for camera " << device->getID() << ": " << ex.what() << RESET << std::endl;
    }
}
