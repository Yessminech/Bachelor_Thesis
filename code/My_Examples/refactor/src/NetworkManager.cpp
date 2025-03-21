#include "Camera.hpp"
#include "NetworkManager.hpp"
#include "DeviceManager.hpp"
#include "GlobalSettings.hpp"

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
#include <fstream>   // Add this for std::ofstream
#include <algorithm> // Add this for std::find

double fpsUpperBound = getGlobalFpsUpperBound();
double fpsLowerBound = getGlobalFpsLowerBound();

NetworkManager::NetworkManager(
    bool debug,
    int ptpSyncTimeout,
    int ptpOffsetThresholdNs,
    int ptpMaxCheck)
    : debug(debug),
      ptpSyncTimeout(ptpSyncTimeout),
      ptpOffsetThresholdNs(ptpOffsetThresholdNs),
      ptpMaxCheck(ptpMaxCheck) {}

NetworkManager::~NetworkManager() {}

void NetworkManager::printPtpConfig(std::shared_ptr<Camera> camera)
{
    try
    {
        PtpConfig ptpConfig = camera->ptpConfig;
        std::cout << GREEN << "Camera ID:                  " << camera->deviceInfos.id << RESET << std::endl;
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

void NetworkManager::monitorPtpStatus(const std::list<std::shared_ptr<Camera>> &openCamerasList, std::atomic<bool> &stopStream)
{
    auto start_time = std::chrono::steady_clock::now();
    bool synchronized = false;
    int numInit = 0;
    int numMaster = 0;
    int numSlave = 0;

    std::cout << "[DEBUG] Starting PTP synchronization monitoring..." << std::endl;

    // Continue until synchronized, timed out, or stopStream is true
    while (!synchronized && !stopStream.load())
    {
        // Reset counters for each check
        numInit = 0;
        numMaster = 0;
        numSlave = 0;

        // Check all cameras
        for (auto &camera : openCamerasList)
        {
            try
            {
                camera->getPtpConfig();
                PtpConfig ptpConfig = camera->ptpConfig;

                // Update status counters
                if (ptpConfig.status == "Master")
                {
                    numMaster++;
                    masterClockId = camera->deviceInfos.id; // Store the master clock ID
                }
                else if (ptpConfig.status == "Slave")
                {
                    numSlave++;
                }
                else
                {
                    numInit++;
                }

                // Print individual camera status if in debug mode
                if (debug)
                {
                    // std::cout << "Camera " << camera->deviceInfos.id << ": "
                    //           << ptpConfig.status << " (Master Clock: " << masterClockId << ")" << std::endl;
                }
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[DEBUG] Error checking PTP status for camera "
                          << camera->deviceInfos.id << ": " << ex.what() << RESET << std::endl;
                numInit++; // Count as initializing if there's an error
            }
        }
        // Check if we have a valid synchronization state:
        // - One master and all others are slaves (normal case)
        // - All slaves and master is external (external clock source)
        if (numMaster == 1 && numSlave == openCamerasList.size() - 1 && numInit == 0)
        {
            std::cout << GREEN << "[DEBUG] PTP synchronization successful!" << RESET << std::endl;
            synchronized = true;
        }

        // Check for timeout
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(ptpSyncTimeout))
        {
            std::cerr << RED << "[DEBUG] PTP synchronization timed out after " << ptpSyncTimeout
                      << " seconds. Current status: " << numInit << " initializing, "
                      << numMaster << " masters, " << numSlave << " slaves" << RESET << std::endl;
            break;
        }

        // If not synchronized yet, wait before checking again
        if (!synchronized)
        {
            std::cout << "[DEBUG] Waiting for PTP synchronization..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }

    // Print final synchronization information
    if (synchronized)
    {
        if (debug)
        {
        std::cout << GREEN << "[DEBUG] PTP synchronized with " << numSlave
                  << " slave cameras and " << numMaster << " master" << RESET << std::endl;}
    }
}

void NetworkManager::logPtpOffset(std::shared_ptr<Camera> camera, int64_t offset)
{
    PtpConfig ptpConfig = camera->ptpConfig;
    std::ofstream logFile("ptp_offsets.log", std::ios_base::app);
    if (logFile.is_open())
    {
        logFile << "Camera ID: " << camera->deviceInfos.id
                << ", Offset: " << offset
                << " ns, Timestamp: " << ptpConfig.timestamp_ns
                << " ns" << std::endl;
        logFile.close();
    }
    else
    {
        std::cerr << "[DEBUG] Unable to open log file to save PTP offset data." << std::endl;
    }
}

void NetworkManager::setOffsetfromMaster(std::shared_ptr<Camera> masterCamera, std::shared_ptr<Camera> camera)
{
    // Get the master camera's timestamp
    masterCamera->getTimestamps();
    uint64_t masterTimestamp = std::stoull(masterCamera->ptpConfig.timestamp_ns);

    // Get the camera's timestamp
    camera->getTimestamps();
    uint64_t cameraTimestamp = std::stoull(camera->ptpConfig.timestamp_ns);

    // Calculate the offset
    int64_t offset = std::abs(static_cast<int64_t>(masterTimestamp) - static_cast<int64_t>(cameraTimestamp));

    // Set the offset
    try
    {
        if (camera->ptpConfig.offsetFromMaster == 0)
        {
            auto nodemap = camera->nodemap;
            rcg::setInteger(nodemap, "PtpOffsetFromMaster", offset);
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "[DEBUG] Error setting offset for camera " << camera->deviceInfos.id << ": " << ex.what() << RESET << std::endl;
    }
}

void NetworkManager::monitorPtpOffset(const std::list<std::shared_ptr<Camera>> &openCamerasList, std::atomic<bool> &stopStream)
{
    // Define threshold for acceptable PTP offset
    bool allSynced = false;
    int checkCount = 0;

    if (debug)
    {
        std::cout << YELLOW << "[DEBUG] Monitoring PTP offset synchronization..." << RESET << std::endl;
    }

    // First, find the master camera from the list based on the masterClockId
    std::shared_ptr<Camera> masterCamera = nullptr;
    for (auto &cam : openCamerasList)
    {
        cam->getPtpConfig();
        if (cam->ptpConfig.status == "Master")
        {
            masterCamera = cam;
            masterClockId = cam->deviceInfos.id;
            break;
        }
    }

    if (!masterCamera)
    {
        std::cerr << RED << "[DEBUG] Error: No master camera found in the camera list" << RESET << std::endl;
        return;
    }

    while (!allSynced && checkCount < ptpMaxCheck && !stopStream.load())
    {
        allSynced = true; // Assume all cameras are in sync until proven otherwise
        checkCount++;

        if (debug)
        {
            std::cout << YELLOW << "[DEBUG] PTP offset check #" << checkCount << RESET << std::endl;
        }

        for (auto &camera : openCamerasList)
        {
            try
            {
                // Update PTP configuration
                camera->getPtpConfig();
                setOffsetfromMaster(masterCamera, camera);
                // Get offset from master
                PtpConfig ptpConfig = camera->ptpConfig;
                int64_t offset = std::abs(ptpConfig.offsetFromMaster); // Use absolute value

                // Log the offset
                logPtpOffset(camera, offset);

                // Check if this camera's offset exceeds the threshold
                if (offset > ptpOffsetThresholdNs)
                {
                    allSynced = false;
                    if (debug)
                    {
                        std::cout << RED << "[DEBUG] Camera " << camera->deviceInfos.id
                                  << " offset (" << offset << " ns) exceeds threshold ("
                                  << ptpOffsetThresholdNs << " ns)" << RESET << std::endl;
                    }
                }
                else
                {
                    if (debug)
                    {
                        std::cout << GREEN << "[DEBUG] Camera " << camera->deviceInfos.id
                                  << " offset (" << offset << " ns) within threshold" << RESET << std::endl;
                    }
                }
            }
            catch (const std::exception &ex)
            {
                if (debug)
                {
                    std::cerr << RED << "[DEBUG] Error checking PTP offset for camera "
                              << camera->deviceInfos.id << ": " << ex.what() << RESET << std::endl;
                }
                allSynced = false; // Consider failed checks as not synced
            }
        }

        // If not all cameras are in sync, wait before checking again
        if (!allSynced && checkCount < ptpMaxCheck && !stopStream.load())
        {
            if (debug)
            {
                std::cout << YELLOW << "[DEBUG] Not all cameras are within offset threshold. "
                          << "Waiting before next check..." << RESET << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }

    // Report final status
    if (debug)
    {
        if (allSynced)
        {
            std::cout << GREEN << "[DEBUG] All cameras are within PTP offset threshold. "
                      << "Network is properly synchronized." << RESET << std::endl;
        for (auto &camera : openCamerasList)
        {
            camera->getPtpConfig();
            camera->getTimestamps();
            printPtpConfig(camera);
        }
    }
        else
        {
            std::cerr << RED << "[DEBUG] Warning: Not all cameras could achieve "
                      << "synchronization within the offset threshold after "
                      << ptpMaxCheck << " attempts." << RESET << std::endl;
        }
    }
}

void NetworkManager::enablePtp(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    for (const auto &camera : openCameras)
    {
        camera->setPtpConfig(true);
    }
}

void NetworkManager::disablePtp(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    for (const auto &camera : openCameras)
    {
        camera->setPtpConfig(false);
    }
}

void NetworkManager::calculateMaxFps(const std::list<std::shared_ptr<Camera>> &openCameras, double packetDelay)
{
    double currentMinFps = fpsUpperBound;
    for (const auto &camera : openCameras)
    {
        double deviceLinkSpeedBps = camera->networkConfig.deviceLinkSpeedBps;
        double calculatedFps = camera->calculateFps(deviceLinkSpeedBps, packetSizeB);
        double clampedFps = std::max(fpsLowerBound, std::min(calculatedFps, fpsUpperBound));
        if (clampedFps < currentMinFps) {
            currentMinFps = clampedFps;
        }
    }
    setGlobalFpsUpperBound(currentMinFps);
}

// void NetworkManager::calculateMaxFpsFromExposure(const std::list<std::shared_ptr<Camera>> &openCameras)
// {
//     double currentMinFps = fpsUpperBound;
//     for (const auto &camera : openCameras)
//     {
//         double exposure = camera->cameraConfig.exposure;
//         double fpsFromExposure = 1 / (exposure * 1e-6); // Corrected units: exposure in microseconds
//         double clampedFps = std::max(fpsLowerBound, std::min(fpsFromExposure, fpsUpperBound));
//         if (clampedFps < currentMinFps) {
//             currentMinFps = clampedFps;
//         }
//     }
//     fpsUpperBound = currentMinFps;
// }

void NetworkManager::getMinimumExposure (const std::list<std::shared_ptr<Camera>> &openCameras)
{
    for (const auto &camera : openCameras)
    {
        camera->setExposureTime(exposureTimeMicros); 
    }
    double currentMinExposure = exposureTimeMicros;
    for (const auto &camera : openCameras)
    {
        double exposure = camera->getExposureTime();
        if (exposure < currentMinExposure) {
            currentMinExposure = exposure;
        }
    }
    exposureTimeMicros = currentMinExposure;
}

void NetworkManager::setExposureAndFps(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    fpsUpperBound = getGlobalFpsUpperBound();
    getMinimumExposure(openCameras);
    std::cout << "Setting exposure to " << exposureTimeMicros << " and fps to:" << fpsUpperBound << " accross all cameras." << std::endl;
    for (const auto &camera : openCameras)
    {
        camera->setFps(fpsUpperBound);
        if (exposureTimeMicros <= 1000000/fpsUpperBound)
        {
            camera->setExposureTime(exposureTimeMicros); // time in microseconds
        }
        else
        {
            std::cout << "Exposure Time" << exposureTimeMicros << "exceeds maximal exposure: " << 1000000/fpsUpperBound << std::endl;
            camera->setExposureTime(1000000/fpsUpperBound); // time in microseconds
        }
    }
}

void NetworkManager::configureNetworkFroSyncFreeRun(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    double packetDelayNs;
    for (auto it = openCameras.rbegin(); it != openCameras.rend(); ++it)
    {
        const auto &camera = *it;
        // double deviceLinkSpeedBps = camera->networkConfig.deviceLinkSpeedBps;
        double deviceLinkSpeedBps = 125000000;              //  125000000 Lucid and 100000000 for Basler
        camera->setDeviceLinkThroughput(deviceLinkSpeedBps); // ToDo check if correct
        camera->setPacketSizeB(packetSizeB);
        double camIndex = std::distance(openCameras.begin(), std::find(openCameras.begin(), openCameras.end(), camera));
        double numCams = openCameras.size();
        camera->setBandwidthDelays(camera, camIndex, numCams, packetSizeB, deviceLinkSpeedBps, bufferPercent);
        packetDelayNs = camera->networkConfig.packetDelayNs;
    }
    calculateMaxFps(openCameras, packetDelayNs);
    setExposureAndFps(openCameras);
}

uint32_t ipToDecimal(std::string ip)
{
    std::vector<std::string> octets;
    std::stringstream ss(ip);
    std::string octet;
    while (std::getline(ss, octet, '.'))
    {
        octets.push_back(octet);
    }
    uint32_t decimal = (std::stoi(octets[0]) << 24) + (std::stoi(octets[1]) << 16) + (std::stoi(octets[2]) << 8) + std::stoi(octets[3]);
    return decimal;
}

void NetworkManager::configureActionCommandInterface(const std::list<std::shared_ptr<Camera>> &openCameras, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, std::string triggerSource, uint32_t actionSelector, uint64_t scheduledDelay)
{
    for (const auto &camera : openCameras)
    {
        try
        {
            auto nodemap = camera->nodemap;
            auto destinationIP = ipToDecimal(camera->deviceInfos.currentIP);
            rcg::setInteger(nodemap, "GevActionDestinationIPAddress", destinationIP);
            rcg::setInteger(nodemap, "ActionDeviceKey", actionDeviceKey);
            rcg::setInteger(nodemap, "ActionGroupKey", groupKey);
            rcg::setInteger(nodemap, "ActionGroupMask", groupMask);
            rcg::setBoolean(nodemap, "ActionScheduledTimeEnable", true);

            // Calculate the execution time
            auto now = std::chrono::system_clock::now().time_since_epoch();
            uint64_t currTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
            uint64_t executeTime = currTimeNs + scheduledDelay;
            rcg::setInteger(nodemap, "ActionScheduledTime", executeTime);

            std::cout << "Interface configured to start acquisition on Action Command for Camera ID: "
                      << camera->deviceInfos.id << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "⚠️ Failed to configure Action Command Interface for Camera ID: "
                      << camera->deviceInfos.id << ": " << e.what() << std::endl;
        }
    }
}

void NetworkManager::sendActionCommand(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    for (const auto &camera : openCameras)
    {
        try
        {
            auto nodemap = camera->nodemap;
            rcg::callCommand(nodemap, "ActionCommand");
            std::cout << "Action Command sent to Camera ID: " << camera->deviceInfos.id << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "⚠️ Failed to send Action Command to Camera ID: "
                      << camera->deviceInfos.id << ": " << e.what() << std::endl;
        }
    }
}
