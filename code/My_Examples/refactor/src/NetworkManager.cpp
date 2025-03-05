#include "Camera.hpp"
#include "NetworkManager.hpp"
#include "DeviceManager.hpp"
#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>
#include <GenApi/GenApi.h>

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <iomanip>
#include <signal.h>
#include <atomic>
#include <set>

NetworkManager::NetworkManager()
    : ptp_sync_timeout(800), num_init(0), num_master(0), num_slave(0), master_clock_id(0) {}

NetworkManager::~NetworkManager() {}

void NetworkManager::printPtpConfig(PtpConfig PtpConfig)
{
    try
    {

        std::cout << "PTP Enabled:                " << (PtpConfig.enabled ? "Yes" : "No") << std::endl;
        std::cout << "PTP Status:                 " << PtpConfig.status << std::endl;
        std::cout << "PTP Clock Accuracy:         " << PtpConfig.clockAccuracy << std::endl;
        std::cout << "PTP Offset From Master:     " << PtpConfig.offsetFromMaster << " ns" << std::endl;
        std::cout << "Timestamp:                  " << PtpConfig.timestamp_ns << " ns" << std::endl;
        std::cout << "Timestamp (seconds):        " << PtpConfig.timestamp_s << " s" << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get timestamp: " << ex.what() << RESET << std::endl;
    }
}

void NetworkManager::monitorPtpStatus(std::shared_ptr<Camera> camera, std::shared_ptr<rcg::Interface> interf, int deviceCount)
{
    auto start_time = std::chrono::steady_clock::now();
    while (num_slave != deviceCount - 1 && num_master != 1)
    {   num_init = 0;
        num_master = 0;
        num_slave = 0;
        for (auto &device : interf->getDevices())
        {
            if (device->getVendor() != interf->getParent()->getVendor())
            {
                continue;
            }
            device->open(rcg::Device::CONTROL);
            std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();
            bool deprecatedFeatures = camera->deviceConfig.deprecatedFW;
            camera->getPtpConfig();
            PtpConfig ptpConfig = camera->ptpConfig;
            statusCheck(ptpConfig.status);
            device->close();
        }
        std::cout << "Camera PTP status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(ptp_sync_timeout))
        {
            std::cerr << "Timed out waiting for camera clocks to become PTP camera slaves. Current status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void NetworkManager::configureActionCommandInterface(std::shared_ptr<rcg::Interface> interf, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, std::string triggerSource, uint32_t actionSelector, uint32_t destinationIP)
{

    try
    {
        auto nodemap = interf->getNodeMap();
        rcg::setInteger(nodemap, "GevActionDestinationIPAddress", destinationIP);
        // Set Action Device Key, Group Key, and Group Mask
        rcg::setInteger(nodemap, "ActionDeviceKey", actionDeviceKey);
        rcg::setInteger(nodemap, "ActionGroupKey", groupKey);
        rcg::setInteger(nodemap, "ActionGroupMask", groupMask);
        rcg::setBoolean(nodemap, "ActionScheduledTimeEnable", true);
        // ✅ Calculate the execution time (e.g., 5 seconds from now)
        auto now = std::chrono::system_clock::now().time_since_epoch();
        uint64_t currTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        uint64_t executeTime = currTimeNs + 5000000000ULL; // 5 seconds later
        rcg::setInteger(nodemap, "ActionScheduledTime", executeTime);

        std::cout << "✅ Interface configured to start acquisition on Action Command.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Failed to configure Action Command Interface: " << e.what() << std::endl;
    }
}

void NetworkManager::sendActionCommand(std::shared_ptr<rcg::System> system)
{

    // Fire the Action Command
    try
    {
        auto nodemap = system->getNodeMap();

        rcg::callCommand(nodemap, "ActionCommand");
            std::cout << "✅ Action Command scheduled \n"; // for execution at: " << executeTime << " ns.

    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Failed to send Action Command: " << e.what() << std::endl;
    }

}

void NetworkManager::statusCheck(const std::string &current_status)
{
    if (current_status == "Initializing")
    {
        num_init++;
    }
    else if (current_status == "Master")
    {
        num_master++;
    }
    else if (current_status == "Slave")
    {
        num_slave++;
    }
    else
    {
        std::cerr << RED << "Unknown PTP status: " << current_status << RESET << std::endl;
    }
}

// Calculate packet delay (not used in composite display but provided for completeness).
double NetworkManager::CalculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams)
{
    double buffer = (bufferPercent / 100.0) * (packetSizeB * (1e9 / deviceLinkSpeedBps));
    return (((packetSizeB * (1e9 / deviceLinkSpeedBps)) + buffer) * (numCams - 1));
}

// Calculate packet delay (not used in composite display but provided for completeness).
double NetworkManager::CalculateTransmissionDelayNs(double packetDelayNs, int camIndex)
{
    return packetDelayNs * camIndex;
}

// Set bandwidth parameters on the device.
void NetworkManager::setBandwidth(const std::shared_ptr<rcg::Device> &device, double camIndex, double numCams)
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

