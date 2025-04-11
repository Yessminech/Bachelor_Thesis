#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <GenApi/GenApi.h>
#include <opencv2/opencv.hpp>
#include <rc_genicam_api/buffer.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/pixel_formats.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>

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
  uint64_t timestamp_ns = 0;
  std::string tickFrequency;
  double timestamp_s = 0.0;
  int offsetFromMaster = 0;
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
};

struct CameraConfig
{
  double exposure;
  float gain;
  double width;
  double height;
  PfncFormat_ pixelFormat;
};

class Camera
{
public:
  // === Constructor / Destructor ===
  Camera(std::shared_ptr<rcg::Device> device,
         CameraConfig cameraConfig = {},
         StreamConfig streamConfig = {},
         NetworkConfig networkConfig = {});
  ~Camera();

  // === Streaming ===
  void startStreaming(std::atomic<bool> &stopStream,
                      std::mutex &globalFrameMutex,
                      std::vector<cv::Mat> &globalFrames,
                      int index,
                      bool saveStream);
  void stopStreaming(std::shared_ptr<rcg::Stream> stream);

  // === Setters ===
  void setPtpConfig(bool enable = true);
  void setFreeRunMode();
  void setScheduledActionCommand(uint32_t actionDeviceKey,
    uint32_t groupKey,
    uint32_t groupMask);
  void setPacketSizeB(double size);
  void setBandwidthDelays(const std::shared_ptr<Camera> &camera,
    double camIndex,
    double numCams,
    double packetSizeB,
    double linkSpeedBps,
    double bufferPercent);
  void setFps(double fpsUpperBound);
  void setGain(float gain);
  void setHeight(int height);
  void setWidth(int width);
  void setExposureTime(double exposureTime);
  void setPixelFormat(std::string pixelFormat);


  // === Triggering ===

  void issueScheduledActionCommand(uint32_t actionDeviceKey,
                                   uint32_t groupKey,
                                   uint32_t groupMask,
                                   int64_t delayS,
                                   const std::string &targetIP = "255.255.255.255");

  // === Calculations ===
  double calculateFps(double linkSpeedBps, double packetSizeB);

  // === Getters ===
  double getExposureTime();
  double getFps();
  std::string getCurrentIP();
  std::string getMAC();
  void getPtpConfig();
  void getTimestamps();
  double getLastSavedFps() const { return lastSavedFps; }

  // === Public Members ===
  DeviceInfos deviceInfos;
  CameraConfig cameraConfig;
  PtpConfig ptpConfig;
  NetworkConfig networkConfig;
  StreamConfig streamConfig;
  std::shared_ptr<GenApi::CNodeMapRef> nodemap;

private:
  // === Streaming ===
  void processRawFrame(const cv::Mat &rawFrame, cv::Mat &outputFrame, uint64_t pixelFormat);
  void updateGlobalFrame(std::mutex &globalFrameMutex,
                         std::vector<cv::Mat> &globalFrames,
                         int index,
                         cv::Mat &frame,
                         int &frameCount,
                         std::chrono::steady_clock::time_point &lastTime);
  void processFrame(const rcg::Buffer *buffer, cv::Mat &outputFrame);
  void cleanupStream(std::shared_ptr<rcg::Stream> &stream);
  std::shared_ptr<rcg::Stream> initializeStream();
  void initializeVideoWriter(int width, int height);
  void saveFrameToVideo(cv::VideoWriter &videoWriter, const cv::Mat &frame);
  void saveFrameToPng(const cv::Mat &frame);



  // === Calculations ===
  double calculatePacketDelayNs(double packetSizeB,
                                double linkSpeedBps,
                                double bufferPercent,
                                double numCams);
  double calculateFrameTransmissionCycle(double linkSpeedBps, double packetSizeB);
  double calculateRawFrameSizeB(int width, int height, PfncFormat_ pixelFormat);

  // === Utility Functions ===
  bool isFpsStableForSaving(double tolerance = 1.0) const;
  void adjustFpsDynamically(double fps, double &lastFpsAdjustment);
  bool isDottedIP(const std::string &ip);
  std::string decimalToIP(uint32_t decimalIP);
  std::string hexToIP(const std::string &hexIP);
  std::string decimalToMAC(uint64_t decimalMAC);
  std::string hexToMAC(const std::string &hexMAC);
  PfncFormat_ getPixelFormat(const std::string &format);
  int getBitsPerPixel(PfncFormat_ pixelFormat);
  void logBandwidthDelaysToCSV(const std::string &camID,
                               int64_t packetDelayNs,
                               int64_t transmissionDelayNs);

  // === Private Members ===
  std::shared_ptr<rcg::Device> device;
  unsigned long frameCounter = 0;
  double lastSavedFps = 0.0;
  cv::VideoWriter videoWriter;
};

#endif // CAMERA_HPP
