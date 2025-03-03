#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <rc_genicam_api/device.h>
#include <GenApi/GenApi.h>

#include <string>
#include <memory>
#include <cstdint>

struct DeviceConfig
{
  std::string id;
  std::string vendor;
  std::string model;
  std::string tlType;
  std::string displayName;
  std::string accessStatus;
  std::string serialNumber;
  std::string version;
  std::string currentIP;
  std::string MAC;
  double timestampFrequency;
  bool deprecatedFW;
};

struct PTPConfig
{
  bool enabled;
  std::string status;
  std::string timestamp_ns;
  std::string tickFrequency;
  uint64_t timestamp_s;
  std::string clockAccuracy;
  int offsetFromMaster;
};

class Camera
{
public:
  Camera(std::shared_ptr<rcg::Device> device);

  ~Camera();

  // Camera Configuration
  // ToDo add clear configuration method
  void setCameraConfig(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID);
  void setActionCommandDeviceConfig(std::shared_ptr<rcg::Device> device, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, const char *triggerSource = "Action1", uint32_t actionSelector = 1);
  void setPTPConfig(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures);
  void setBandwidth(const std::shared_ptr<rcg::Device> &device, double camIndex);

  // Network Control
  std::string getCurrentIP(const std::shared_ptr<rcg::Device> &device);
  void getPTPConfig(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures);
  std::string getMAC(const std::shared_ptr<rcg::Device> &device);
  void getTimestamps(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures);

  // Streaming Control
  void startStreaming(); // ToDo

private:
  std::shared_ptr<rcg::Device> device;
  bool streaming;
  DeviceConfig deviceConfig;
  PTPConfig ptpConfig; // ToDo Public?
  double CalculateTransmissionDelayNs(double packetDelayNs, int camIndex);
  double CalculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams);
  std::string decimalToIP(uint32_t decimalIP);
  std::string hexToIP(const std::string &hexIP);
  std::string decimalToMAC(uint64_t decimalMAC);
  std::string hexToMAC(const std::string &hexMAC);
};

#endif // CAMERA_HPP