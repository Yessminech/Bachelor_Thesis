#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <rc_genicam_api/device.h>
#include <GenApi/GenApi.h>
#include <opencv2/opencv.hpp>

#include <string>
#include <memory>
#include <cstdint>
#include <mutex>
#include <vector>

// Todo remove this
#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"

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
  void setCameraConfig();
  void setActionCommandDeviceConfig(std::shared_ptr<rcg::Device> device, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, const char *triggerSource = "Action1", uint32_t actionSelector = 1);
  void setPTPConfig();

  // Network Control
  std::string getCurrentIP();
  void getPTPConfig();
  std::string getMAC();
  void getTimestamps();

  // Streaming Control
  void processRawFrame(const cv::Mat &rawFrame, cv::Mat &outputFrame, uint64_t pixelFormat);
  void startStreaming(bool stop_streaming, std::mutex globalFrameMutex, std::vector<cv::Mat> globalFrames, int index);
  void stopStreaming(std::shared_ptr<rcg::Stream> stream);
   
  bool debug = true;

private:
  std::shared_ptr<rcg::Device> device;
  std::shared_ptr<GenApi::CNodeMapRef> nodemap;
  float exposure = 222063; // Example: 20 ms
  float gain = 0; // Example: Gain of 10 dB
  DeviceConfig deviceConfig; // ToDo Public?
  PTPConfig ptpConfig; // ToDo Public?
  std::string decimalToIP(uint32_t decimalIP);
  std::string hexToIP(const std::string &hexIP);
  std::string decimalToMAC(uint64_t decimalMAC);
  std::string hexToMAC(const std::string &hexMAC);
};

#endif // CAMERA_HPP