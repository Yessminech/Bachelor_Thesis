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
#include <atomic>
#include <thread>
#include <sys/stat.h>
#include <chrono>
#include <unordered_map>
#include <stdexcept>

#include <ctime>
// Add this helper function to convert string to enum
PfncFormat_ Camera::stringToPixelFormat(const std::string& format)
{
    // Map common format strings to enum values
    if (format == "BGR8") return BGR8;
    if (format == "RGB8") return RGB8;
    if (format == "RGBa8") return RGBa8;
    if (format == "BGRa8") return BGRa8;
    if (format == "Mono8") return Mono8;
    if (format == "Mono16") return Mono16;
    if (format == "BayerRG8") return BayerRG8;
    if (format == "BayerBG8") return BayerBG8;
    if (format == "BayerGB8") return BayerGB8;
    if (format == "BayerGR8") return BayerGR8;
    if (format == "YCbCr422_8") return YCbCr422_8;
    if (format == "YUV422_8") return YUV422_8;
    
    // Default case if no match found
    std::cerr << RED << "Unknown pixel format string: " << format << RESET << std::endl;
    return Mono8; // Return a default format as fallback
}
Camera::Camera(std::shared_ptr<rcg::Device> device) : device(device)
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
        }
        nodemap = device->getRemoteNodeMap();
        setCameraConfig();
        resetTimestamp();
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
        width = rcg::getInteger(nodemap, "Width");
        height = rcg::getInteger(nodemap, "Height");
        std::string formatString = rcg::getEnum(nodemap, "PixelFormat");
        pixelFormat = stringToPixelFormat(formatString);
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
            //rcg::setFloat(nodemap, "ExposureTimeAbs", exposure); // ToDo for all or only deprecated ? 
        }
        // rcg::setEnum(nodemap, "ExposureAuto","Continuous");
        // rcg::setFloat(nodemap, "ExposureTime", exposure); // Example: 20 ms
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

void Camera::setPtpConfig(bool enable)
{
    std::string feature = deviceConfig.deprecatedFW ? "GevIEEE1588" : "PtpEnable";

    try
    {
        if (enable)
        {rcg::setString(nodemap, feature.c_str(), "true", enable);
        rcg::setInteger(nodemap, "DeviceLinkSpeed", 1250000000);
        ptpConfig.enabled = rcg::getBoolean(nodemap, feature.c_str());
        ptpConfig.deviceLinkSpeed = rcg::getInteger(nodemap, "DeviceLinkSpeed");
        if (debug)
        {
            std::cout << GREEN << "[DEBUG] Camera " << device->getID() << feature << " set to:" << ptpConfig.enabled << ", DeviceLinkSpeed is set to " << ptpConfig.deviceLinkSpeed << RESET << std::endl;
        }
    }
    else
    {
        rcg::setString(nodemap, feature.c_str(), "false", enable);
        ptpConfig.enabled = rcg::getBoolean(nodemap, feature.c_str());
        if (debug)
        {
            std::cout << GREEN << "[DEBUG] Camera " << device->getID() << feature << " set to:" << ptpConfig.enabled << RESET << std::endl;
        }
    }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to set PTP: " << ex.what() << RESET << std::endl;
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
            rcg::callCommand(nodemap, "PtpDataSetLatch");
            ptpConfig.enabled = rcg::getBoolean(nodemap, "PtpEnable");
            ptpConfig.status = rcg::getString(nodemap, "PtpStatus");
            ptpConfig.clockAccuracy = rcg::getString(nodemap, "PtpClockAccuracy");
            ptpConfig.offsetFromMaster = rcg::getInteger(nodemap, "PtpOffsetFromMaster");
        }
        else
        {
            rcg::callCommand(nodemap, "GevIEEE1588DataSetLatch");
            ptpConfig.enabled = rcg::getBoolean(nodemap, "GevIEEE1588");
            ptpConfig.status = rcg::getString(nodemap, "GevIEEE1588Status");
            ptpConfig.clockAccuracy = "Unavailable";
            try {
                // Basler Cameras 
                ptpConfig.offsetFromMaster = rcg::getInteger(nodemap, "GevIEEE1588OffsetFromMaster");
            } catch (const std::exception &ex) {
                ptpConfig.offsetFromMaster = 0; // Unavailable
            }
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



void Camera::resetTimestamp(){

    try
    {
        setPtpConfig(false);
        if (!deviceConfig.deprecatedFW)
        {
            rcg::callCommand(nodemap, "TimestampReset");
        }
        else
        {
            rcg::callCommand(nodemap, "GevTimestampControlReset");
        }
        if (debug)
        {
            std::cout << GREEN << "Timestamp reset success" << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to reset timestamp: " << ex.what() << RESET << std::endl;
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
        }
        else
        {
            rcg::callCommand(nodemap, "GevTimestampControlLatch");
            ptpConfig.timestamp_ns = rcg::getString(nodemap, "GevTimestampValue");
        }
        ptpConfig.timestamp_s = std::stod(ptpConfig.timestamp_ns) / 1e9;

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

            if (debug)
                std::cout << GREEN << "[DEBUG] Camera " << deviceConfig.id << ": AcquisitionMode:" << rcg::getEnum(nodemap, "AcquisitionMode") << ": TriggerSelector:" << rcg::getEnum(nodemap, "TriggerSelector") << RESET << std::endl;
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
    double timestampSec = static_cast<double>(timestampNS) / 1e9; // ToDo double Check

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

void Camera::initializeVideoWriter(const std::string &directory, int width, int height)
{
    // Store the dimensions we want to use
    videoWidth = width;
    videoHeight = height;

    // Ensure even dimensions (required by many codecs)
    videoWidth += (videoWidth % 2);
    videoHeight += (videoHeight % 2);

    // Create output directory
    struct stat st;
    if (stat(directory.c_str(), &st) != 0)
    {
        if (mkdir(directory.c_str(), 0777) != 0)
        {
            std::cerr << RED << "Error: Unable to create output directory " << directory << RESET << std::endl;
            return;
        }
    }

    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&now_time_t);
    std::ostringstream timestampStream;
    timestampStream << std::put_time(&tm, "%Y%m%d_%H%M%S");

    std::string sanitizedId = deviceConfig.id;
    // Remove any characters that might be problematic in filenames
    std::replace(sanitizedId.begin(), sanitizedId.end(), '(', '_');
    std::replace(sanitizedId.begin(), sanitizedId.end(), ')', '_');
    std::replace(sanitizedId.begin(), sanitizedId.end(), ' ', '_');
    
    std::string outputPath;
    bool videoOpened = false;
    
    // Try different codecs in order of preference
    std::vector<std::pair<std::string, int>> codecOptions = {
        {".mp4", cv::VideoWriter::fourcc('a', 'v', 'c', '1')},   // H.264
        {".avi", cv::VideoWriter::fourcc('X', 'V', 'I', 'D')},   // XVID
        {".avi", cv::VideoWriter::fourcc('M', 'J', 'P', 'G')},   // MJPG
        {".avi", cv::VideoWriter::fourcc('D', 'I', 'V', 'X')}    // DIVX
    };
    
    double fps = 30.0; // ToDo make this configurable
    
    for (const auto& codecOption : codecOptions) {
        std::string extension = codecOption.first;
        int codec = codecOption.second;
        
        std::string outputFilename = "camera_" + sanitizedId + "_" + timestampStream.str() + extension;
        outputPath = directory + "/" + outputFilename;
        
        if (debug)
            std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ":Trying codec: " << ((char)(codec & 0xFF)) 
                     << ((char)((codec >> 8) & 0xFF)) 
                     << ((char)((codec >> 16) & 0xFF)) 
                     << ((char)((codec >> 24) & 0xFF)) 
                     << " for file: " << outputPath << RESET << std::endl;
        
        videoWriter.open(outputPath, codec, fps, cv::Size(videoWidth, videoHeight), true);
        
        if (videoWriter.isOpened()) {
            videoOpened = true;
            break;
        } else {
            std::cerr << RED << "Failed to open video with codec: " << ((char)(codec & 0xFF)) 
                     << ((char)((codec >> 8) & 0xFF)) 
                     << ((char)((codec >> 16) & 0xFF)) 
                     << ((char)((codec >> 24) & 0xFF)) 
                     << RESET << std::endl;
        }
    }
    
    if (!videoOpened) {
        std::cerr << RED << "Error: All codecs failed. Video recording disabled." << RESET << std::endl;
        return;
    }

    if (debug) {
        // Write a test frame to ensure the writer actually works
        cv::Mat testFrame(videoHeight, videoWidth, CV_8UC3, cv::Scalar(0, 255, 0));
        std::string text = "Camera: " + deviceConfig.id;
        cv::putText(testFrame, text, cv::Point(30, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
        videoWriter.write(testFrame);
    }

    videoOutputPath = outputPath;
    std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": Video recording started at: " << outputPath << RESET << std::endl;
}

void Camera::saveFrameToVideo(cv::VideoWriter &videoWriter, const cv::Mat &frame)
{
    if (!videoWriter.isOpened()) {
        std::cerr << RED << "Error: Video writer is not opened." << RESET << std::endl;
        return;
    }

    // Ensure frame is not empty and is a valid image format
    if (frame.empty()) {
        std::cerr << RED << "Error: Cannot save empty frame to video." << RESET << std::endl;
        return;
    }

    // Check if frame is grayscale and convert to color if needed
    cv::Mat colorFrame;
    if (frame.channels() == 1) {
        cv::cvtColor(frame, colorFrame, cv::COLOR_GRAY2BGR);
    } else {
        colorFrame = frame;
    }

    // Ensure frame has the correct size
    cv::Mat resizedFrame;
    if (colorFrame.cols != videoWidth || colorFrame.rows != videoHeight) {
        try {
            cv::resize(colorFrame, resizedFrame, cv::Size(videoWidth, videoHeight));
        } catch (const std::exception &ex) {
            std::cerr << RED << "Failed to resize frame for video: " << ex.what() << RESET << std::endl;
            return;
        }
    } else {
        resizedFrame = colorFrame;
    }
    
    // Write frame to video
    try {
        videoWriter.write(resizedFrame);
        frameCounter++;
    } catch (const std::exception &ex) {
        std::cerr << RED << "Exception writing to video: " << ex.what() << RESET << std::endl;
    }
}

void Camera::startStreaming(std::atomic<bool> &stopStream, std::mutex &globalFrameMutex, std::vector<cv::Mat> &globalFrames, int index, bool saveStream)
{
    setFreeRunMode();
    auto stream = initializeStream();
    if (!stream)
        return;

    if (saveStream)
    {
        initializeVideoWriter(videoOutputPath, 640, 480);
    }

    auto lastTime = std::chrono::steady_clock::now();
    int frameCount = 0;
    int consecutiveFailures = 0; // Count failed grabs

    while (!stopStream.load())
    {
        const rcg::Buffer *buffer = nullptr;
        try
        {
            buffer = stream->grab(500);
            if (!buffer)
            {
                consecutiveFailures++;
                if (consecutiveFailures > 10)
                {
                    std::cerr << RED << "Error: Too many failed frame grabs for camera " << device->getID()
                              << ". Stopping stream..." << RESET << std::endl;
                    break; // Exit loop if grabbing fails too often
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Avoid tight loop
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

        consecutiveFailures = 0; // Reset failure count
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
            if (saveStream && videoWriter.isOpened())
            {
                try
                {
                    saveFrameToVideo(videoWriter, outputFrame);
                }
                catch (const std::exception &ex)
                {
                    std::cerr << RED << "Exception during video writing: " << ex.what() << RESET << std::endl;
                }
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

    if (saveStream && videoWriter.isOpened())
    {
        try
        {
            videoWriter.release();
            std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": saveStream success" << RESET << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Exception during video writer release: " << ex.what() << RESET << std::endl;
        }
    }
}

// Function to stopStream streaming
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

double Camera::calculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams) // ToDo for chunkdata
{
    double buffer = (bufferPercent / 100.0) * (packetSizeB * (1e9 / deviceLinkSpeedBps));
    return (((packetSizeB * (1e9 / deviceLinkSpeedBps)) + buffer) * (numCams - 1));
}

double Camera::getPacketDelayNs()
{
    return packetDelayNs;
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
        return 16;  // 4:2:2 subsampling means 16 bits per pixel
        
    default:
        std::cerr << RED << "Unknown pixel format: " << pixelFormat << RESET << std::endl;
        return 0;
    }
}

double Camera::calculateRawFrameSizeB(int width, int height, PfncFormat_ pixelFormat) {
    int bitsPerPixel = getBitsPerPixel(pixelFormat);
    int totalPixels = width * height;
    int totalBits = totalPixels * bitsPerPixel;

    return static_cast<double>(totalBits) / 8.0;  // Convert bits to bytes
}

double Camera::calculateFrameTransmissionCycle(double deviceLinkSpeedBps, double packetSizeB)
{
    double RawFrameSizeBytes = calculateRawFrameSizeB(width, height, pixelFormat);
    double numPacketPerFrameB = RawFrameSizeBytes / packetSizeB;
    double frameTransmissionCycle = RawFrameSizeBytes / deviceLinkSpeedBps;
    return frameTransmissionCycle + numPacketPerFrameB * packetDelayNs * 1e-9; // ToDo check if correct
}

double Camera::calculateFps(double deviceLinkSpeedBps, double packetSizeB)
{
    double frameTransmissionCycle = calculateFrameTransmissionCycle(deviceLinkSpeedBps, packetSizeB);
    return std::floor(1 / frameTransmissionCycle);
}

void Camera::setDeviceLinkThroughput(double deviceLinkSpeedBps)
{
    try
    {
        rcg::setEnum(nodemap, "DeviceLinkThroughputLimitMode", "On");
        rcg::setInteger(nodemap, "DeviceLinkThroughputLimit", deviceLinkSpeedBps);
        if (debug)
        {
            std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": DeviceLinkThroughputLimit set to:" << rcg::getInteger(nodemap, "DeviceLinkThroughputLimit") << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to set DeviceLinkThroughputLimit: " << ex.what() << RESET << std::endl;
    }
}

void Camera::setPacketSizeB(double packetSizeB){
    try
    {
        packetSizeB = std::ceil(packetSizeB / 4.0) * 4; // Ensure packet size is a multiple of 4
        rcg::setInteger(nodemap, "GevSCPSPacketSize", packetSizeB); 
        if (debug)
        {
            std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": GevSCPSPacketSize set to:" << rcg::getInteger(nodemap, "GevSCPSPacketSize") << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to set GevSCPSPacketSize: " << ex.what() << RESET << std::endl;
    }
}


// Set bandwidth parameters on the device.
void Camera::setBandwidthDelays(const std::shared_ptr<Camera> &camera, double camIndex, double numCams, double packetSizeB, double deviceLinkSpeedBps, double bufferPercent)
{
    try
    {
        packetDelayNs = calculatePacketDelayNs(packetSizeB, deviceLinkSpeedBps, bufferPercent, numCams);
        transmissionDelayNs = packetDelayNs * camIndex;
        int64_t gevSCPDValue = static_cast<int64_t>(packetDelayNs);
        rcg::setInteger(camera->nodemap, "GevSCPD", gevSCPDValue); // in counter units, 1ns resolution if ptp is enabled
        int64_t gevSCFTDValue = static_cast<int64_t>(transmissionDelayNs);
        rcg::setInteger(camera->nodemap, "GevSCFTD", gevSCFTDValue); // in counter units, 1ns resolution if ptp is enabled
        

        if (debug)
            std::cout << "[DEBUG] numCams and CamIndex: " << numCams - 1 << " " << camIndex << std::endl;
        std::cout << "[DEBUG] Camera " << device->getID() << ": Calculated packet delay: " << packetDelayNs << " ns" << std::endl;
        std::cout << "[DEBUG] Camera " << device->getID() << ": Calculated transmission delay: " << transmissionDelayNs << " ns" << std::endl;
        std::cout << "[DEBUG] Camera " << device->getID() << ": GevSCPD set to: " << rcg::getInteger(nodemap, "GevSCPD") << " ns" << std::endl;
        std::cout << "[DEBUG] Camera " << device->getID() << ": GevSCFTD set to: " << rcg::getInteger(nodemap, "GevSCFTD") << " ns" << std::endl;
        std::cout << GREEN << "[DEBUG] Camera " << device->getID() << ": setBandwidthDelays sucess" << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to set bandwidth for camera " << device->getID() << ": " << ex.what() << RESET << std::endl;
    }
}

void Camera::setFps(double maxFps)
{
    try
    {
        rcg::setBoolean(nodemap, "AcquisitionFrameRateEnable", true);
        // if(!camera->deviceConfig.deprecatedFW)
            rcg::setEnum(nodemap, "AcquisitionFrameRate", "FrameRate");
        // else{
            rcg::setEnum(nodemap, "AcquisitionFrameRateAbs", "FrameRate"); // ToDo for all or only deprecated ? 
        // }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to set frame rate for camera " << deviceConfig.id << ": " << e.what() << std::endl;
    }

    if (debug)
        std::cout << GREEN << "[DEBUG] Camera " << deviceConfig.id << "fps set to:" << rcg::getFloat(nodemap, "AcquisitionFrameRate") << maxFps << std::endl;
}

void Camera::setExposureTime(double exposureTime) // ToDo correct this
{
    try
    {
        rcg::setEnum(nodemap, "ExposureMode", "Timed");
        rcg::setEnum(nodemap, "ExposureAuto", "Off");
        rcg::setFloat(nodemap, "ExposureTime", exposureTime);
        rcg::setFloat(nodemap, "ExposureTimeAbs", exposureTime); // ToDo for all or only deprecated ? 

        if (debug)
            std::cout << GREEN << "[DEBUG] Camera " << deviceConfig.id << "ExposureTime set to:" << rcg::getFloat(nodemap, "ExposureTimeAbs") << std::endl; //ExposureTime // ToDo for all or only deprecated ? 
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to set exposure time for camera " << deviceConfig.id << ": " << e.what() << std::endl;
    }
}