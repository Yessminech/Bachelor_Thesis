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
#include <signal.h>
#include <atomic>
#include <set>

#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"
#define CYAN "\033[36m"

    // Map from camera ID to a deque of offset values
    struct CameraSample {
        int64_t offset_ns;
        uint64_t timestamp_ns;
    };
class NetworkManager
{
public:
NetworkManager(
    bool debug = true,
    int ptpSyncTimeout = 800,
    int ptpOffsetThresholdNs = 100000,
    int ptpMaxCheck = 40);
    ~NetworkManager();

    void enablePtp(const std::list<std::shared_ptr<Camera>> &openCameras);
    void disablePtp(const std::list<std::shared_ptr<Camera>> &openCameras);

    void printPtpConfig(std::shared_ptr<Camera> camera);
    void monitorPtpStatus(const std::list<std::shared_ptr<Camera>> &openCamerasList,  std::atomic<bool>& stopStream);
    void monitorPtpOffset(const std::list<std::shared_ptr<Camera>> &openCamerasList,  std::atomic<bool>& stopStream);
    void configureMultiCamerasNetwork(const std::list<std::shared_ptr<Camera>> &openCameras);
    // void logPtpOffset(std::shared_ptr<Camera> camera, int64_t offset);
    void setOffsetfromMaster(std::shared_ptr<Camera> masterCamera, std::shared_ptr<Camera> camera);
    void setExposureAndFps(const std::list<std::shared_ptr<Camera>> &openCameras);
    void configureActionCommandInterface(const std::list<std::shared_ptr<Camera>> &openCameras, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, std::string triggerSource, uint32_t actionSelector, uint64_t scheduledDelay);
    void sendActionCommand(const std::list<std::shared_ptr<Camera>> &openCameras);
    void calculateMaxFps(const std::list<std::shared_ptr<Camera>> &openCameras, double packetDelay);
    // void calculateMaxFpsFromExposure(const std::list<std::shared_ptr<Camera>> &openCameras);
    void getMinimumExposure (const std::list<std::shared_ptr<Camera>> &openCameras);
    void writeOffsetHistoryToCsv(
        const std::unordered_map<std::string, std::deque<CameraSample>> &offsetHistory);   bool debug = true;

private:
// ToDo values here make no sense, on construcotor they are set to correct values
    double exposureTimeMicros = 100000;
    double packetSizeB = 9000; // Jumbo frames defined on hardaware (Todo check max)
    double bufferPercent = 10.93; // 10.93; // ToDo How to set this value 
    int ptpSyncTimeout = 800; // 800 ms
    int ptpMaxCheck = 20; // 10 checks
    const int timeWindowSize = 20; // 10 seconds //ToDo check unit
    int ptpOffsetThresholdNs = 100000; // 1 ms
    std::string masterClockId;
    int64_t scheduledDelayS = 1; // 1 second
    
};
#endif // NETWORKMANAGER_HPP