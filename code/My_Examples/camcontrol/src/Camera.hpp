/**
 * @brief Library component of the TU Berlin industrial automation framework
 *
 * Copyright (c) 2025
 * TU Berlin, Institut für Werkzeugmaschinen und Fabrikbetrieb
 * Fachgebiet Industrielle Automatisierungstechnik
 * Author: Yessmine Chabchoub
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written permission.
 *
 * @note
 * DISCLAIMER
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DAMAGES ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE.
 */

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

/**
 * @brief Contains device information.
 */
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

/**
 * @brief Configuration for Precision Time Protocol (PTP) synchronization.
 */
struct PtpConfig
{
  bool enabled;
  std::string status;
  uint64_t timestamp_ns = 0;
  std::string tickFrequency;
  double timestamp_s = 0.0;
  int offsetFromMaster = 0;
};

/**
 * @brief Network configuration parameters.
 */
struct NetworkConfig
{
  double deviceLinkSpeedBps;
  double packetSizeB;
  double bufferPercent;
  double packetDelayNs = 0;
  double transmissionDelayNs = 0;
  double fps;
};

/**
 * @brief Camera-specific configuration settings.
 */
struct CameraConfig
{
  double exposure;
  float gain;
  double width;
  double height;
  PfncFormat_ pixelFormat;
};

/**
 * @brief Represents a camera device and manages its operations.
 */
class Camera
{
public:
  /**
   * @brief Constructs a Camera object.
   */
  Camera(std::shared_ptr<rcg::Device> device,
         CameraConfig cameraConfig = {},
         NetworkConfig networkConfig = {});
  ~Camera();

  /**
   * @brief Starts streaming frames from the camera.
   */
  void startStreaming(std::atomic<bool> &stopStream,
                      std::mutex &globalFrameMutex,
                      std::vector<cv::Mat> &globalFrames,
                      int index,
                      bool saveStream);

  /**
   * @brief Configure PTP on camera.
   */
  void setPtpConfig(bool enable = true);

  /**
   * @brief Sets the camera to free-run mode.
   */
  void setFreeRunMode();

  /**
   * @brief Configures scheduled action command triggering.
   */
  void setScheduledActionCommand(uint32_t actionDeviceKey,
                                 uint32_t groupKey,
                                 uint32_t groupMask);

  /**
   * @brief Sets the packet size for transmission.
   */
  void setPacketSizeB(double size);

  /**
   * @brief Configures bandwidth delay parameters for the camera.
   */
  void setBandwidthDelays(const std::shared_ptr<Camera> &camera,
                          double camIndex,
                          double numCams,
                          double packetSizeB,
                          double linkSpeedBps,
                          double bufferPercent);

  /**
   * @brief Sets the maximum allowed frames per second.
   */
  void setFps(double fpsUpperBound);

  /**
   * @brief Sets the camera gain value.
   */
  void setGain(float gain);

  /**
   * @brief Sets the image height.
   */
  void setHeight(int height);

  /**
   * @brief Sets the image width.
   */
  void setWidth(int width);

  /**
   * @brief Sets the camera exposure time.
   */
  void setExposureTime(double exposureTime);

  /**
   * @brief Sets the pixel format.
   */
  void setPixelFormat(std::string pixelFormat);

  /**
   * @brief Issues a scheduled action trigger command in Lucid Cameras.
   */
  void issueScheduledActionCommandLucid(uint32_t actionDeviceKey,
                                        uint32_t groupKey,
                                        uint32_t groupMask,
                                        int64_t delayS,
                                        const std::string &targetIP = "255.255.255.255");

  /**
   * @brief Calculates the achievable frames per second.
   */
  double calculateFps(double linkSpeedBps, double packetSizeB);

  /**
   * @brief Gets the current exposure time.
   */
  double getExposureTime();

  /**
   * @brief Gets the configured frames per second.
   */
  double getFps();

  /**
   * @brief Returns the current IP address.
   */
  std::string getCurrentIP();

  /**
   * @brief Returns the MAC address.
   */
  std::string getMAC();

  /**
   * @brief Fetches PTP configuration details.
   */
  void getPtpConfig();

  /**
   * @brief Retrieves the current timestamps from the camera.
   */
  void getTimestamps();

  /**
   * @brief Returns the last saved FPS value.
   */
  double getLastSavedFps() const { return lastSavedFps; }

  DeviceInfos deviceInfos;
  CameraConfig cameraConfig;
  PtpConfig ptpConfig;
  NetworkConfig networkConfig;
  std::shared_ptr<GenApi::CNodeMapRef> nodemap;

private:
  /**
   * @brief Processes a raw frame into an output frame.
   */
  void processRawFrame(const cv::Mat &rawFrame, cv::Mat &outputFrame, uint64_t pixelFormat);

  /**
   * @brief Updates the global frame buffer.
   */
  void updateGlobalFrame(std::mutex &globalFrameMutex,
                         std::vector<cv::Mat> &globalFrames,
                         int index,
                         cv::Mat &frame,
                         int &frameCount,
                         std::chrono::steady_clock::time_point &lastTime);

  /**
   * @brief Processes a frame received from the device.
   */
  void processFrame(const rcg::Buffer *buffer, cv::Mat &outputFrame);

  /**
   * @brief Cleans up the streaming resources.
   */
  void cleanupStream(std::shared_ptr<rcg::Stream> &stream);

  /**
   * @brief Initializes a stream from the device.
   */
  std::shared_ptr<rcg::Stream> initializeStream();

  /**
   * @brief Saves a frame as a PNG file.
   */
  void saveFrameToPng(const cv::Mat &frame);

  /**
   * @brief Calculates packet delay based on network parameters.
   */
  double calculatePacketDelayNs(double packetSizeB,
                                double linkSpeedBps,
                                double bufferPercent,
                                double numCams);

  /**
   * @brief Calculates the frame transmission cycle time.
   */
  double calculateFrameTransmissionCycle(double linkSpeedBps, double packetSizeB);

  /**
   * @brief Calculates the size of a raw frame in bytes.
   */
  double calculateRawFrameSizeB(int width, int height, PfncFormat_ pixelFormat);

  /**
   * @brief Checks if FPS is stable enough for saving.
   */
  bool isFpsStableForSaving(double tolerance = 1.0) const;

  /**
   * @brief Adjusts FPS dynamically to maintain target values.
   */
  void adjustFpsDynamically(double fps, double &lastFpsAdjustment);

  /**
   * @brief Checks if an IP string is dotted decimal format.
   */
  bool isDottedIP(const std::string &ip);

  /**
   * @brief Converts a decimal IP address to string.
   */
  std::string decimalToIP(uint32_t decimalIP);

  /**
   * @brief Converts a hexadecimal IP address to string.
   */
  std::string hexToIP(const std::string &hexIP);

  /**
   * @brief Converts a decimal MAC address to string.
   */
  std::string decimalToMAC(uint64_t decimalMAC);

  /**
   * @brief Converts a hexadecimal MAC address to string.
   */
  std::string hexToMAC(const std::string &hexMAC);

  /**
   * @brief Returns pixel format from format string.
   */
  PfncFormat_ getPixelFormat(const std::string &format);

  /**
   * @brief Returns the number of bits per pixel for a pixel format.
   */
  int getBitsPerPixel(PfncFormat_ pixelFormat);

  /**
   * @brief Logs bandwidth delays to a CSV file.
   */
  void logBandwidthDelaysToCSV(const std::string &camID,
                               int64_t packetDelayNs,
                               int64_t transmissionDelayNs);

  std::shared_ptr<rcg::Device> device;
  unsigned long frameCounter = 0;
  double lastSavedFps = 0.0;
};
#endif // CAMERA_HPP
