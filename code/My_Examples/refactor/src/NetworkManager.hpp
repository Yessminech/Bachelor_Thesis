#ifndef NETWORKMANAGER_HPP
#define NETWORKMANAGER_HPP

#include "Camera.hpp"
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
#include <csignal>
#include <atomic>
#include <set>
#include <unordered_map>
#include <deque>

struct CameraSample
{
    int64_t offset_ns;
    uint64_t timestamp_ns;
};

class NetworkManager
{
public:
    // === Constructor / Destructor ===
    NetworkManager();
    ~NetworkManager();

    // === PTP Control ===
    void enablePtp(const std::list<std::shared_ptr<Camera>> &openCameras);
    void disablePtp(const std::list<std::shared_ptr<Camera>> &openCameras);
    void monitorPtpStatus(const std::list<std::shared_ptr<Camera>> &openCamerasList);
    void monitorPtpOffset(const std::list<std::shared_ptr<Camera>> &openCamerasList);
    void setOffsetfromMaster(std::shared_ptr<Camera> masterCamera, std::shared_ptr<Camera> camera);

    // === Bandwidth Configuration ===
    void configureBandwidth(const std::list<std::shared_ptr<Camera>> &openCameras);
    void calculateMaxFps(const std::list<std::shared_ptr<Camera>> &openCameras, double packetDelay);
    void getMinimumExposure(const std::list<std::shared_ptr<Camera>> &openCameras);
    void setExposureAndFps(const std::list<std::shared_ptr<Camera>> &openCameras);
    void configurePtpSyncFreeRun(const std::list<std::shared_ptr<Camera>> &openCamerasList);

    // === Logging & Visualization ===
    void printPtpConfig(std::shared_ptr<Camera> camera);
    void logOffsetHistoryToCSV(
        const std::unordered_map<std::string, std::deque<CameraSample>> &offsetHistory);
    void plotOffsets(double ptpThreshold = 1000.0);

private:
    // === Private Members ===
    const int timeWindowSize = 20; // Sample buffer window (ToDo unit?)
    std::string masterClockId;
};

#endif // NETWORKMANAGER_HPP