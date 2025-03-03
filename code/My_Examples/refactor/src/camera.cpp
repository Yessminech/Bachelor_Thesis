#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <sstream>
#include <set>
#include <arpa/inet.h>
#include <iomanip>
#include <regex>

Camera::Camera(std::shared_ptr<rcg::Device> device) : device(device)
{
    if (device)
    {
        setCameraConfig(device->getRemoteNodeMap(), device->getID());
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

void Camera::setCameraConfig(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID)
{
    // Ideally: Load config from xml file
    DeviceConfig config;
    config.id = device->getID();
    config.vendor = device->getVendor();
    config.model = device->getModel();
    config.tlType = device->getTLType();
    config.displayName = device->getDisplayName();
    config.accessStatus = device->getAccessStatus();
    config.serialNumber = device->getSerialNumber();
    config.version = device->getVersion();
    config.timestampFrequency = device->getTimestampFrequency();
    config.currentIP = getCurrentIP(device);
    config.MAC = getMAC(device);
    // config.deprecatedFW
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
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Failed to configure Action Command Device: " << e.what() << std::endl;
    }
}

// ToDo add a ptpcheck method that checks if ptp is true on all devices and there are masters and slave it
void Camera::setPTPConfig(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures)
{
    std::string feature = deprecatedFeatures ? "GevIEEE1588" : "PtpEnable";

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

// Calculate packet delay (not used in composite display but provided for completeness).
double Camera::CalculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams)
{
    double buffer = (bufferPercent / 100.0) * (packetSizeB * (1e9 / deviceLinkSpeedBps));
    return (((packetSizeB * (1e9 / deviceLinkSpeedBps)) + buffer) * (numCams - 1));
}

// Calculate packet delay (not used in composite display but provided for completeness).
double Camera::CalculateTransmissionDelayNs(double packetDelayNs, int camIndex)
{
    return packetDelayNs * camIndex;
}

// Set bandwidth parameters on the device.
void Camera::setBandwidth(const std::shared_ptr<rcg::Device> &device, double camIndex)
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

std::string Camera::getCurrentIP(const std::shared_ptr<rcg::Device> &device)
{
    device->open(rcg::Device::CONTROL);
    auto nodemap = device->getRemoteNodeMap();
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

        device->close();
        return currentIP;
    }
    catch (const std::exception &ex)
    {
        device->close();
        std::cerr << "Exception: " << ex.what() << std::endl;
        return "";
    }
}

void Camera::getPTPConfig(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures)
{
    try
    {
        if (!deprecatedFeatures)
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

std::string Camera::getMAC(const std::shared_ptr<rcg::Device> &device)
{
    std::regex macPattern(R"(^([0-9A-Fa-f]{2}:){5}([0-9A-Fa-f]{2})$)");

    device->open(rcg::Device::CONTROL);
    auto nodemap = device->getRemoteNodeMap();
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
        device->close();
        return MAC;
    }
    catch (const std::exception &ex)
    {
        device->close();
        std::cerr << "Exception: " << ex.what() << std::endl;
        return "";
    }
}

void Camera::getTimestamps(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures)
{
    try
    {
        if (!deprecatedFeatures)
        {
            rcg::callCommand(nodemap, "TimestampLatch");
            ptpConfig.timestamp_ns = rcg::getString(nodemap, "TimestampLatchValue");
            ptpConfig.tickFrequency = "1000000000"; // Assuming 1 GHz if PTP is used
        }
        else
        {
            rcg::callCommand(nodemap, "GevTimestampControlLatch");
            ptpConfig.timestamp_ns = rcg::getString(nodemap, "GevTimestampValue");
            ptpConfig.tickFrequency = rcg::getString(nodemap, "GevTimestampTickFrequency"); // if PTP is used, this feature must return 1,000,000,000 (1 GHz).
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

void Camera::startStreaming()
{
    // ToDo
}