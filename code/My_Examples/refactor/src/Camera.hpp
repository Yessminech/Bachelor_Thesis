#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "rc_genicam_api/pixel_formats.h"
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/buffer.h>
#include <GenApi/GenApi.h>
#include <opencv2/opencv.hpp>

#include <string>
#include <memory>
#include <cstdint>
#include <mutex>
#include <vector>

// Color definitions (consider removing if not needed)
#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"

// Structs for configuration and device information
struct DeviceInfos
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

struct PtpConfig
{
  bool enabled;
  std::string status;
  std::string timestamp_ns;
  std::string tickFrequency;
  uint64_t timestamp_s;
  std::string clockAccuracy;
  int offsetFromMaster;
};

struct NetworkConfig
{
  double deviceLinkSpeedBps;
  double packetSizeB;
  double bufferPercent;
  double packetDelayNs = 0;
  double transmissionDelayNs = 0;
  double fps;
};

struct StreamConfig
{
  int videoWidth = 640;
  int videoHeight = 480;
  std::string videoOutputPath = "output/"; // Default directory
};

struct CameraConfig
{
  double exposure;
  float gain;
  double width;
  double height;
  PfncFormat_ pixelFormat;
};

// Camera class definition
class Camera
{
public:
  // Constructor and Destructor
  Camera(std::shared_ptr<rcg::Device> device, bool debug = true,
         CameraConfig cameraConfig = {}, StreamConfig streamConfig = {}, NetworkConfig networkConfig = {});
  ~Camera();

  // Calculations
  double calculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams);
  double calculateFrameTransmissionCycle(double deviceLinkSpeedBps, double packetSizeB);
  double calculateRawFrameSizeB(int width, int height, PfncFormat_ pixelFormat);
  double calculateFps(double deviceLinkSpeedBps, double packetSizeB);

  // Streaming Control
  void startStreaming(std::atomic<bool> &stopStream, std::mutex &globalFrameMutex, std::vector<cv::Mat> &globalFrames, int index, bool saveStream);
  void stopStreaming(std::shared_ptr<rcg::Stream> stream);

  // Getters
  std::string getCurrentIP();
  std::string getMAC();
  void getPtpConfig();
  void getTimestamps();

  // Setters
  void resetTimestamp();
  void setDeviceInfos();
  void setPtpConfig(bool enable = true);
  void setDeviceLinkThroughput(double deviceLinkSpeedBps);
  void setPacketSizeB(double packetSizeB);
  void setBandwidthDelays(const std::shared_ptr<Camera> &camera, double camIndex, double numCams, double packetSizeB, double deviceLinkSpeedBps, double bufferPercent);
  void setFps(double fpsUpperBound);
  void setExposureTime(double exposureTime);

  void setActionCommandDeviceConfig(std::shared_ptr<rcg::Device> device, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, const char *triggerSource = "Action1", uint32_t actionSelector = 1);

  // Public Members
  DeviceInfos deviceInfos;
  PtpConfig ptpConfig;
  NetworkConfig networkConfig;
  StreamConfig streamConfig;
  CameraConfig cameraConfig;
  std::shared_ptr<GenApi::CNodeMapRef> nodemap;

private:
  // Private Members
  std::shared_ptr<rcg::Device> device;
  bool debug = true;
  unsigned long frameCounter = 0;
  cv::VideoWriter videoWriter; // Used to write frames to a video file

  // Private Methods
  void initializeVideoWriter(const std::string &directory, int width, int height);
  void setFreeRunMode();
  void cleanupStream(std::shared_ptr<rcg::Stream> &stream);
  std::shared_ptr<rcg::Stream> initializeStream();
  const rcg::Buffer *grabFrame(std::shared_ptr<rcg::Stream> &stream);
  void processFrame(const rcg::Buffer *buffer, cv::Mat &outputFrame);
  void processRawFrame(const cv::Mat &rawFrame, cv::Mat &outputFrame, uint64_t pixelFormat);
  void updateGlobalFrame(std::mutex &globalFrameMutex, std::vector<cv::Mat> &globalFrames, int index,
                         cv::Mat &frame, int &frameCount, std::chrono::steady_clock::time_point &lastTime);
  void saveFrameToVideo(cv::VideoWriter &videoWriter, const cv::Mat &frame);

  // Utility Methods
  std::string decimalToIP(uint32_t decimalIP);
  std::string hexToIP(const std::string &hexIP);
  std::string decimalToMAC(uint64_t decimalMAC);
  std::string hexToMAC(const std::string &hexMAC);
  PfncFormat_ getPixelFormat(const std::string &format);
  int getBitsPerPixel(PfncFormat_ pixelFormat);
};

#endif // CAMERA_HPP