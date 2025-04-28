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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NETWORKMANAGER_HPP
#define NETWORKMANAGER_HPP

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
#include <csignal>
#include <atomic>
#include <set>
#include <unordered_map>
#include <deque>

/**
 * @brief Struct to represent a sample containing offset and timestamp information for a camera.
 */
struct CameraSample
{
    int64_t offset_ns;     ///< Offset to master clock in nanoseconds
    uint64_t timestamp_ns; ///< Timestamp when sample was taken
};

/**
 * @brief NetworkManager class handles camera synchronization, PTP configuration, bandwidth optimization, and logging.
 */
class NetworkManager
{
public:
    /**
     * @brief Constructs a NetworkManager instance.
     */
    NetworkManager();
    ~NetworkManager();

    /**
     * @brief Enables PTP (Precision Time Protocol) synchronization on a list of open cameras.
     * @param openCameras List of camera objects.
     */
    void enablePtp(const std::list<std::shared_ptr<Camera>> &openCameras);

    /**
     * @brief Disables PTP synchronization on a list of open cameras.
     * @param openCameras List of camera objects.
     */
    void disablePtp(const std::list<std::shared_ptr<Camera>> &openCameras);

    /**
     * @brief Monitors the PTP synchronization status of open cameras.
     * @param openCamerasList List of camera objects.
     */
    void monitorPtpStatus(const std::list<std::shared_ptr<Camera>> &openCamerasList);

    /**
     * @brief Monitors and logs PTP offsets over time.
     * @param openCamerasList List of camera objects.
     */
    void monitorPtpOffset(const std::list<std::shared_ptr<Camera>> &openCamerasList);

    /**
     * @brief Sets a specific camera's offset based on a master camera's clock for cameras with missing offset feature.
     * @param masterCamera The master camera.
     * @param camera The target camera to synchronize.
     */
    void setOffsetfromMaster(std::shared_ptr<Camera> masterCamera, std::shared_ptr<Camera> camera);

    /**
     * @brief Configures network bandwidth settings.
     * @param openCameras List of camera objects.
     */
    void configureBandwidth(const std::list<std::shared_ptr<Camera>> &openCameras);

    /**
     * @brief Calculates the theoretical maximum frames per second (FPS) based on packet delay.
     * @param openCameras List of camera objects.
     * @param packetDelay Delay in packet transmission (in microseconds).
     */
    void calculateMaxFps(const std::list<std::shared_ptr<Camera>> &openCameras, double packetDelay);

    /**
     * @brief Retrieves the real minimum exposure time set on the cameras.
     * @param openCameras List of camera objects.
     */
    void getMinimumExposure(const std::list<std::shared_ptr<Camera>> &openCameras);

    /**
     * @brief Sets the exposure and frame rate of the cameras based on bandwidth and exposure constraints.
     * @param openCameras List of camera objects.
     */
    void setExposureAndFps(const std::list<std::shared_ptr<Camera>> &openCameras);

    /**
     * @brief Configures cameras for PTP-synchronized free-run streaming.
     * @param openCamerasList List of camera objects.
     */
    void configurePtpSyncFreeRun(const std::list<std::shared_ptr<Camera>> &openCamerasList);

    /**
     * @brief Prints the PTP configuration details of a camera.
     * @param camera Camera object.
     */
    void printPtpConfig(std::shared_ptr<Camera> camera);

    /**
     * @brief Logs the history of PTP offsets to a CSV file.
     * @param offsetHistory Map containing the history of offsets for each camera.
     */
    void logOffsetHistoryToCSV(
        const std::unordered_map<std::string, std::deque<CameraSample>> &offsetHistory);

    /**
     * @brief Plots the offset trends over time for PTP synchronization analysis.
     * @param ptpThreshold Threshold value for highlighting offset deviations (in nanoseconds).
     */
    void plotOffsets(double ptpThreshold = 1000.0);

private:
    /**
     * @brief Defines the window size for offset monitoring (number of samples).
     */
    const int timeWindowSize = 20;

    /**
     * @brief Identifier of the master clock used for synchronization.
     */
    std::string masterClockId;
};
#endif // NETWORKMANAGER_HPP
