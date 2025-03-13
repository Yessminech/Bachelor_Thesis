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
    : ptpSyncTimeout(800), numInit(0), numMaster(0), numSlave(0), masterClockId(0) {}

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

void NetworkManager::monitorPtpStatus(std::shared_ptr<Camera> camera, std::shared_ptr<rcg::Interface> interf, int deviceCount, std::list<std::shared_ptr<Camera>> &openCamerasList)
{
    auto start_time = std::chrono::steady_clock::now();
    while (numSlave != deviceCount - 1 && numMaster != 1)
    {   numInit = 0;
        numMaster = 0;
        numSlave = 0;
        for (auto &camera : openCamerasList)
        {
            camera->getPtpConfig();
            PtpConfig ptpConfig = camera->ptpConfig;
            statusCheck(ptpConfig.status);
        }
        // std::cout << "Camera PTP status: " << numInit << " initializing, " << numMaster << " masters, " << numSlave << " slaves" << std::endl;
        // if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(ptpSyncTimeout))
        // {
        //     std::cerr << "Timed out waiting for camera clocks to become PTP camera slaves. Current status: " << numInit << " initializing, " << numMaster << " masters, " << numSlave << " slaves" << std::endl;
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
        numInit++;
    }
    else if (current_status == "Master")
    {
        numMaster++;
    }
    else if (current_status == "Slave")
    {
        numSlave++;
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
        // camera->setFps(maxFrameRate);
        camera->setPtpConfig();
        double camIndex = std::distance(OpenCameras.begin(), std::find(OpenCameras.begin(), OpenCameras.end(), camera)); 
        double numCams = OpenCameras.size();
        camera->setBandwidth(camera, camIndex, numCams, deviceLinkSpeedBps, packetSizeB, bufferPercent);
    }
}