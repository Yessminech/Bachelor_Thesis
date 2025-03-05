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

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    void statusCheck(const std::string &current_status);
    void printPtpConfig(PtpConfig PtpConfig);
    void monitorPtpStatus(std::shared_ptr<Camera> camera, std::shared_ptr<rcg::Interface> interf, int deviceCount);
    void configureActionCommandInterface(std::shared_ptr<rcg::Interface> interf, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, std::string triggerSource = "Action1", uint32_t actionSelector = 1, uint32_t destinationIP = 0xFFFFFFFF);
    void sendActionCommand(std::shared_ptr<rcg::System> system);
    void setBandwidth(const std::shared_ptr<rcg::Device> &device, double camIndex);
    double CalculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams);
    double CalculateTransmissionDelayNs(double packetDelayNs, int camIndex);

    // Additional methods
    void setPtpConfig();                   // Enables Precision Time Protocol for synchronization
    void disablePTP();                     // Disables PTP sync
   // NetworkStatus getNetworkStatus();      // Returns network status
    void setBandwidth(const std::shared_ptr<rcg::Device> &device, double camIndex, double numCams); // Allocates bandwidth for a camera
    std::string monitorPtpStatus();          // Checks PTP synchronization health
    float calculateTransmissionDelay(int cameraId); // Estimates transmission delay
    bool debug = true; 
    
private:
    int ptp_sync_timeout;
    int num_init;
    int num_master;
    int num_slave;
    int64_t master_clock_id;
};


