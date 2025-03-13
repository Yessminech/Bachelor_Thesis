#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <rc_genicam_api/device.h>
#include <rc_genicam_api/buffer.h>
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

struct PtpConfig
{
  bool enabled;
  std::string status;
  std::string timestamp_ns;
  std::string tickFrequency;
  uint64_t timestamp_s;
  std::string clockAccuracy;
  int offsetFromMaster;
  double deviceLinkSpeed;
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
  void setPtpConfig();
  void setBandwidth(const std::shared_ptr<Camera> &camera, double camIndex, double numCams, double packetSizeB, double deviceLinkSpeedBps, double bufferPercent);
  void setFps(double maxFrameRate);

  // Network Control
  std::string getCurrentIP();
  void getPtpConfig();
  std::string getMAC();
  void getTimestamps();

  // Streaming Control
  void startStreaming(std::atomic<bool> &stopStream, std::mutex &globalFrameMutex, std::vector<cv::Mat> &globalFrames, int index, bool saveStream);
  void stopStreaming(std::shared_ptr<rcg::Stream> stream);

  bool debug = true;
  DeviceConfig deviceConfig; // ToDo Public?
  PtpConfig ptpConfig;       // ToDo Public?
  std::shared_ptr<GenApi::CNodeMapRef> nodemap;

private:
  cv::VideoWriter videoWriter;        // Used to write frames to a video file
  int videoWidth = 640;
  int videoHeight = 480;
  unsigned long frameCounter = 0;
  std::string videoOutputPath = "output/"; // Default directory

  std::shared_ptr<rcg::Device> device;
  float exposure = 222063; // Example: 20 ms
  float gain = 0;          // Example: Gain of 10 dB
  void initializeVideoWriter(const std::string &directory, int width, int height);
  void setFreeRunMode();
  double CalculateTransmissionDelayNs(double packetDelayNs, int camIndex);
  double CalculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams);
  float calculateTransmissionDelay(int cameraId); // Estimates transmission delay
  void cleanupStream(std::shared_ptr<rcg::Stream> &stream);
  std::shared_ptr<rcg::Stream> initializeStream();
  const rcg::Buffer *grabFrame(std::shared_ptr<rcg::Stream> &stream);
  void processFrame(const rcg::Buffer *buffer, cv::Mat &outputFrame);
  void processRawFrame(const cv::Mat &rawFrame, cv::Mat &outputFrame, uint64_t pixelFormat);
  void updateGlobalFrame(std::mutex &globalFrameMutex, std::vector<cv::Mat> &globalFrames, int index,
                         cv::Mat &frame, int &frameCount, std::chrono::steady_clock::time_point &lastTime);
  void saveFrameToVideo(cv::VideoWriter &videoWriter, const cv::Mat &frame);
  std::string decimalToIP(uint32_t decimalIP);
  std::string hexToIP(const std::string &hexIP);
  std::string decimalToMAC(uint64_t decimalMAC);
  std::string hexToMAC(const std::string &hexMAC);
};

#endif // CAMERA_HPP