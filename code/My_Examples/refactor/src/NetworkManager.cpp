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

std::list<std::shared_ptr<Camera>> openCameras; // ToDo import from Device manager and add to constructor

NetworkManager::NetworkManager()
    : ptp_sync_timeout(800), num_init(0), num_master(0), num_slave(0), master_clock_id(0) {}

NetworkManager::~NetworkManager() {}

void NetworkManager::printPtpConfig(std::shared_ptr<Camera> camera)
{
    try
    {
        PtpConfig ptpConfig = camera->ptpConfig;
        std::cout << "PTP Enabled:                " << (ptpConfig.enabled ? "Yes" : "No") << std::endl;
        std::cout << "PTP Status:                 " << ptpConfig.status << std::endl;
        std::cout << "PTP Clock Accuracy:         " << ptpConfig.clockAccuracy << std::endl;
        std::cout << "PTP Offset From Master:     " << ptpConfig.offsetFromMaster << " ns" << std::endl;
        std::cout << "Timestamp:                  " << ptpConfig.timestamp_ns << " ns" << std::endl;
        std::cout << "Timestamp (seconds):        " << ptpConfig.timestamp_s << " s" << std::endl;
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
        for (auto &camera : openCameras)
        {
            camera->getPtpConfig();
            PtpConfig ptpConfig = camera->ptpConfig;
            statusCheck(ptpConfig.status);
        }
        // std::cout << "Camera PTP status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
        // if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(ptp_sync_timeout))
        // {
        //     std::cerr << "Timed out waiting for camera clocks to become PTP camera slaves. Current status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
        //     break;
        // }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

// void NetworkManager::configureActionCommandInterface(std::shared_ptr<rcg::Interface> interf, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, std::string triggerSource, uint32_t actionSelector, uint32_t destinationIP)
// {

//     try
//     {
//         auto nodemap = interf->getNodeMap();
//         rcg::setInteger(nodemap, "GevActionDestinationIPAddress", destinationIP);
//         // Set Action Device Key, Group Key, and Group Mask
//         rcg::setInteger(nodemap, "ActionDeviceKey", actionDeviceKey);
//         rcg::setInteger(nodemap, "ActionGroupKey", groupKey);
//         rcg::setInteger(nodemap, "ActionGroupMask", groupMask);
//         rcg::setBoolean(nodemap, "ActionScheduledTimeEnable", true);
//         // ✅ Calculate the execution time (e.g., 5 seconds from now)
//         auto now = std::chrono::system_clock::now().time_since_epoch();
//         uint64_t currTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
//         uint64_t executeTime = currTimeNs + 5000000000ULL; // 5 seconds later
//         rcg::setInteger(nodemap, "ActionScheduledTime", executeTime);

//         std::cout << "✅ Interface configured to start acquisition on Action Command.\n";
//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "⚠️ Failed to configure Action Command Interface: " << e.what() << std::endl;
//     }
// }

// void NetworkManager::sendActionCommand(std::shared_ptr<rcg::System> system)
// {

//     // Fire the Action Command
//     try
//     {
//         auto nodemap = system->getNodeMap();

//         rcg::callCommand(nodemap, "ActionCommand");
//             std::cout << "✅ Action Command scheduled \n"; // for execution at: " << executeTime << " ns.

//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "⚠️ Failed to send Action Command: " << e.what() << std::endl;
//     }

// }

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

void NetworkManager::configureNetworkFroSyncFreeRun(const std::list<std::shared_ptr<Camera>> &OpenCameras)
{
    for (const auto &camera : OpenCameras)
    {
        camera->setPtpConfig();
        double camIndex = std::distance(OpenCameras.begin(), std::find(OpenCameras.begin(), OpenCameras.end(), camera)); 
        double numCams = OpenCameras.size();
        camera->setBandwidth(camera, camIndex, numCams, deviceLinkSpeedBps, packetSizeB, bufferPercent);
    }
}