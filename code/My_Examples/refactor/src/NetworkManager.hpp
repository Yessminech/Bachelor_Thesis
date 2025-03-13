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
    NetworkManager();
    ~NetworkManager();

    void printPtpConfig(std::shared_ptr<Camera> camera);
    void monitorPtpStatus(std::shared_ptr<Camera> camera, std::shared_ptr<rcg::Interface> interf, int deviceCount, std::list<std::shared_ptr<Camera>> &openCamerasList);
    void configureActionCommandInterface(std::shared_ptr<rcg::Interface> interf, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, std::string triggerSource = "Action1", uint32_t actionSelector = 1, uint32_t destinationIP = 0xFFFFFFFF);
    void sendActionCommand(std::shared_ptr<rcg::System> system);
    void configureNetworkFroSyncFreeRun(const std::list<std::shared_ptr<Camera>> &OpenCameras);

    // Additional methods
   // void disablePTP();                                                                              // Disables PTP sync
    // NetworkStatus getNetworkStatus();      // Returns network status
    bool debug = true;

private:
    double maxFrameRate = 10; // rcg::getFloat(nodemap, "AcquisitionFrameRate");
    double deviceLinkSpeedBps = 1250000000; // 1 Gbps // ToDo What does this mean and how to define this
    double packetSizeB = 9012;             // ToDo What does this mean and how to define this, jumbo frames?
    double bufferPercent = 15;             // 10.93;
    int ptpSyncTimeout;
    int numInit;
    int numMaster;
    int numSlave;
    int64_t masterClockId;

    void statusCheck(const std::string &current_status);
};
