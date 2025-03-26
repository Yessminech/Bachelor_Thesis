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
#include <fstream>   
#include <algorithm> 
#include <deque>    
#include <random>

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
        std::cout << "PTP Offset From Master:     " << ptpConfig.offsetFromMaster << " ns" << std::endl;
        std::cout << "Timestamp (ns):                  " << ptpConfig.timestamp_ns << " ns" << std::endl;
        std::cout << "Timestamp (s):        " << ptpConfig.timestamp_s << " s" << std::endl;
        std::cout << std::endl;
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
                      << " slave cameras and " << numMaster << " master" << RESET << std::endl;
        }
    }
}

// void NetworkManager::logPtpOffset(std::shared_ptr<Camera> camera, int64_t offset)
// {
//     PtpConfig ptpConfig = camera->ptpConfig;
//     std::ofstream logFile("ptp_offsets.log", std::ios_base::app);
//     if (logFile.is_open())
//     {
//         logFile << "Camera ID: " << camera->deviceInfos.id
//                 << ", Offset: " << offset
//                 << " ns, Timestamp: " << camera->ptpConfig.timestamp_ns
//                 << " ns" << std::endl;
//         logFile.close();
//     }
//     else
//     {
//         std::cerr << "[DEBUG] Unable to open log file to save PTP offset data." << std::endl;
//     }
// }

void NetworkManager::setOffsetfromMaster(std::shared_ptr<Camera> masterCamera, std::shared_ptr<Camera> camera)
{
    // Get the master camera's timestamp
    masterCamera->getTimestamps();
    uint64_t masterTimestamp = masterCamera->ptpConfig.timestamp_ns;

    // Get the camera's timestamp
    camera->getTimestamps();
    uint64_t cameraTimestamp = camera->ptpConfig.timestamp_ns;

    // Calculate the offset
    int64_t offset = masterTimestamp- cameraTimestamp;

    // Set the offset
    try
    {
        if (camera->ptpConfig.offsetFromMaster == 0 && camera->deviceInfos.id != masterClockId)
        {
            auto nodemap = camera->nodemap;
            camera->ptpConfig.offsetFromMaster = offset;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "[DEBUG] Error setting offset for camera " << camera->deviceInfos.id << ": " << ex.what() << RESET << std::endl;
    }
}

void NetworkManager::writeOffsetHistoryToCsv(
    const std::unordered_map<std::string, std::deque<CameraSample>> &offsetHistory)
{
    const std::string filename = "ptp_offset_history.csv";
    std::ofstream outFile(filename);

    if (!outFile.is_open())
    {
        std::cerr << RED << "[DEBUG] Failed to open CSV file for writing: " << filename << RESET << std::endl;
        return;
    }

    // Header
    outFile << "Sample";
    for (const auto &[camId, _] : offsetHistory)
    {
        outFile << "," << camId << "_timestamp_ns," << camId << "_offset_ns";
    }
    outFile << "\n";

    // Determine max rows
    size_t maxLength = 0;
    for (const auto &[_, history] : offsetHistory)
        maxLength = std::max(maxLength, history.size());

    // Write rows
    for (size_t i = 0; i < maxLength; ++i)
    {
        outFile << i;
        for (const auto &[_, history] : offsetHistory)
        {
            if (i < history.size())
            {
                outFile << "," << history[i].timestamp_ns << "," << history[i].offset_ns;
            }
            else
            {
                outFile << ",,";
            }
        }
        outFile << "\n";
    }

    outFile.close();
    std::cout << CYAN << "[DEBUG] Offset + timestamp history written to " << filename << RESET << std::endl;
}

void NetworkManager::monitorPtpOffset(const std::list<std::shared_ptr<Camera>> &openCamerasList, std::atomic<bool> &stopStream)
{
    bool allSynced = false;
    int checkCount = 0;

    if (debug)
    {
        std::cout << YELLOW << "[DEBUG] Monitoring PTP offset synchronization..." << RESET << std::endl;
    }

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


    
    std::unordered_map<std::string, std::deque<CameraSample>> offsetWindowHistory; // For stability check
    std::unordered_map<std::string, std::vector<CameraSample>> offsetLogHistory;   // For logging to CSV
        
    while (!allSynced && checkCount < ptpMaxCheck && !stopStream.load())
    {
        allSynced = true;
        checkCount++;

        if (debug)
        {
            std::cout << YELLOW << "[DEBUG] PTP offset check #" << checkCount << RESET << std::endl;
        }

        for (auto &camera : openCamerasList)
        {
            try
            {
                camera->getTimestamps();
                camera->getPtpConfig();
        
                std::string camId = camera->deviceInfos.id;
                int64_t offset = (camId == masterClockId) ? 0 : std::abs(camera->ptpConfig.offsetFromMaster);
                uint64_t timestamp = camera->ptpConfig.timestamp_ns;
                offsetLogHistory[camId].push_back(CameraSample{offset, timestamp});
                auto &window = offsetWindowHistory[camId];
                if (window.size() >= timeWindowSize)
                window.pop_front();
                window.push_back(CameraSample{offset, timestamp});
        
                // Only evaluate stability for non-master cameras
                if (camId != masterClockId && window.size() == timeWindowSize)
                {
                    bool inWindow = std::all_of(window.begin(), window.end(),
                        [&](const CameraSample &sample) { return sample.offset_ns <= ptpOffsetThresholdNs; });
        
                    if (!inWindow)
                    {
                        allSynced = false;
                        if (debug)
                            std::cout << RED << "[DEBUG] Camera " << camId
                                      << " not stable within time window." << RESET << std::endl;
                    }
                    else if (debug)
                    {
                        std::cout << GREEN << "[DEBUG] Camera " << camId
                                  << " stable within time window." << RESET << std::endl;
                    }
                }
                else if (camId != masterClockId)
                {
                    allSynced = false;
                    if (debug)
                    {
                        std::cout << YELLOW << "[DEBUG] Camera " << camId
                                  << " collecting samples (" << window.size() << "/" << timeWindowSize << ")" << RESET << std::endl;
                    }
                }
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[DEBUG] Error checking PTP offset for camera "
                          << camera->deviceInfos.id << ": " << ex.what() << RESET << std::endl;
                allSynced = false;
            }
        }
        if (!allSynced && checkCount < ptpMaxCheck && !stopStream.load())
        {
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }
    std::unordered_map<std::string, std::deque<CameraSample>> converted;

    for (const auto& [key, vec] : offsetLogHistory)
    {
        converted[key] = std::deque<CameraSample>(vec.begin(), vec.end());
    }
    
    writeOffsetHistoryToCsv(converted);
    if (debug)
    {
        if (allSynced)
        {
            std::cout << GREEN << "[DEBUG] All cameras are stable and synchronized within the time window." << RESET << std::endl;
            for (auto &camera : openCamerasList)
            {
                camera->getTimestamps();
                printPtpConfig(camera);
            }
        }
        else
        {
            std::cerr << RED << "[DEBUG] Synchronization failed: not all cameras were stable within threshold over "
                      << timeWindowSize << " consecutive samples." << RESET << std::endl;
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
        if (clampedFps < currentMinFps)
        {
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

void NetworkManager::getMinimumExposure(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    for (const auto &camera : openCameras)
    {
        camera->setExposureTime(exposureTimeMicros);
    }
    double currentMinExposure = exposureTimeMicros;
    for (const auto &camera : openCameras)
    {
        double exposure = camera->getExposureTime();
        if (exposure < currentMinExposure)
        {
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
        if (exposureTimeMicros <= 1000000 / fpsUpperBound)
        {
            camera->setExposureTime(exposureTimeMicros); // time in microseconds
        }
        else
        {
            std::cout << "Exposure Time" << exposureTimeMicros << "exceeds maximal exposure: " << 1000000 / fpsUpperBound << std::endl;
            camera->setExposureTime(1000000 / fpsUpperBound); // time in microseconds
        }
    }
}

void NetworkManager::configureMultiCamerasNetwork(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    double packetDelayNs;
    for (auto it = openCameras.rbegin(); it != openCameras.rend(); ++it)
    {
        const auto &camera = *it;
        // double deviceLinkSpeedBps = camera->networkConfig.deviceLinkSpeedBps;
        double deviceLinkSpeedBps = 125000000;               //  125000000 Lucid and 100000000 for Basler
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

