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

nt ptp_sync_timeout = 800; // ToDo Set or compute this value
int num_init = 0;
int num_master = 0;
int num_slave = 0;
int64_t master_clock_id = 0;
void statusCheck(const std::string &current_status);
void printPtpConfig(PTPConfig ptpConfig);
void monitorPtpStatus(std::shared_ptr<rcg::Interface> interf, int deviceCount);
void configureActionCommandInterface(std::shared_ptr<rcg::Interface> interf, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, std::string triggerSource = "Action1", uint32_t actionSelector = 1, uint32_t destinationIP = 0xFFFFFFFF);
void sendActionCommand(std::shared_ptr<rcg::System> system);
void setBandwidth(const std::shared_ptr<rcg::Device> &device, double camIndex);
double CalculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams);
double CalculateTransmissionDelayNs(double packetDelayNs, int camIndex);
// + setPTPConfig()                   // Enables Precision Time Protocol for synchronization
// + disablePTP()                  // Disables PTP sync
// + getNetworkStatus() : NetworkStatus // Returns network status
// + setBandwidth(cameraId, bandwidth) // Allocates bandwidth for a camera
// + monitorPtpStatus() : PTPStatus  // Checks PTP synchronization health
// + calculateTransmissionDelay(cameraId) : float // Estimates transmission delay
