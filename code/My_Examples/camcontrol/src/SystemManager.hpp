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
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without specific prior written permission.
 *
 * @note
 * DISCLAIMER
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SYSTEM_MANAGER_HPP
#define SYSTEM_MANAGER_HPP
#include "DeviceManager.hpp"
#include "NetworkManager.hpp"
#include "StreamManager.hpp"
#include <list>
#include <string>
#include <atomic>
#include <chrono>

/**
 * @brief SystemManager class is the interface for the overall management of the system and its components.
 * It provides methods to initialize the system, enumerate cameras, set features, and manage streaming and PTP.
 */
class SystemManager
{
public:
  /**
    Constructs a systemManager class by initializing all connected GenTl devices.
  */
  SystemManager();
  ~SystemManager();

  /**
    Enumerates all connected cameras and stores their IDs in availableCamerasIds.
  */
  void enumerateCameras();
  /**
    Enumerates open cameras and lists their IDs.
    @param cameraIDs List of camera IDs to enumerate.
  */
  void enumerateOpenCameras(std::list<std::string> camerasIDs); // Enumerates open cameras

  /**
    Sets a feature for a specific camera or all cameras if no cameraID is provided.
    @param featureName Name of the feature to set (e.g., "width", "height", "gain", "exposure_time", "pixel_format").
    @param value Value to set for the feature.
    @param cameraID ID of the camera to set the feature for. If empty, sets the feature for all cameras.
    @return true if the feature was set successfully, false otherwise.
  */
 bool setFeature(std::string featureName, std::string value, std::list<std::string> cameraIDs = {});

  /**
    Starts synchronized streaming for the specified cameras.
    @param camerasIDs List of camera IDs to start streaming for.
    @param saveStream Flag indicating whether to save the stream.
    @param acquisitionDelay Delay in milliseconds before starting the acquisition.
  */
  void syncFreeRunStream(std::list<std::string> camerasIDs, bool saveStream, std::chrono::milliseconds acquisitionDelay);

  /**
    Enables PTP synchronization for the specified cameras.
    @param camerasIDs List of camera IDs to enable PTP for.
    @return true if PTP was enabled successfully, false otherwise.
  */
  bool ptpEnable(std::list<std::string> camerasIDs);
  /**
    Disables PTP synchronization for the specified cameras.
    @param camerasIDs List of camera IDs to disable PTP for.
    @return true if PTP was disabled successfully, false otherwise.
  */
  bool ptpDisable(std::list<std::string> camerasIDs);

  std::atomic<bool> stopStream{false};
  DeviceManager deviceManager;
  NetworkManager networkManager;
  StreamManager streamManager;
  std::list<std::string> availableCamerasIds;
};

#endif // SYSTEM_MANAGER_HPP