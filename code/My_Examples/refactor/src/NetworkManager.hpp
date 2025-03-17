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

class NetworkManager
{
public:
NetworkManager(
    bool debug,
    int ptpSyncTimeout,
    int ptpOffsetThresholdNs,
    int ptpMaxCheck);
    ~NetworkManager();

    void enablePtp(const std::list<std::shared_ptr<Camera>> &openCameras);
    void disablePtp(const std::list<std::shared_ptr<Camera>> &openCameras);

    void printPtpConfig(std::shared_ptr<Camera> camera);
    void monitorPtpStatus(const std::list<std::shared_ptr<Camera>> &openCamerasList,  std::atomic<bool>& stopStream);
    void monitorPtpOffset(const std::list<std::shared_ptr<Camera>> &openCamerasList,  std::atomic<bool>& stopStream);
    void configureNetworkFroSyncFreeRun(const std::list<std::shared_ptr<Camera>> &openCameras);
    void logPtpOffset(std::shared_ptr<Camera> camera, int64_t offset);
    void setOffsetfromMaster(std::shared_ptr<Camera> masterCamera, std::shared_ptr<Camera> camera);
    void setBandwidthDelaysSharing(const std::list<std::shared_ptr<Camera>> &openCameras);
    void sendActionCommand(std::shared_ptr<rcg::System> system);
    void calculateMaxFps(const std::list<std::shared_ptr<Camera>> &openCameras, double packetDelay);

    bool debug = true;

private:
    double maxFps = 1000; // rcg::getFloat(nodemap, "AcquisitionFrameRate");
    double packetSizeB = 9000; // Jumbo frames defined on hardaware (Todo check max)
    double bufferPercent = 15; // 10.93; // ToDo How to set this value 
    int ptpSyncTimeout = 800; // 800 ms
    int ptpMaxCheck = 10; // 10 checks
    int ptpOffsetThresholdNs = 1000; // 1 us
    std::string masterClockId;
};