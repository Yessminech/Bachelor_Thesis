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

#ifndef STREAMMANAGER_HPP
#define STREAMMANAGER_HPP

#include <opencv2/opencv.hpp>

#include "Camera.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <list>

/**
 * @brief StreamManager class is responsible for managing the streaming of images from multiple cameras.
 * It handles the threading, nchronized acquisition and the stream display for the camera streams.
 */
class StreamManager
{
public:
    /**
      Constructs a systemManager class by initializing all threads.
    */
    StreamManager();
    ~StreamManager();

    /**
     * @brief Launches a camera streaming thread in free-run mode.
     * @param camera The camera to stream from.
     * @param stopStream Atomic boolean to control the streaming process.
     * @param saveStream Boolean to indicate if the stream should be saved to disk.
     * @param threadIndex Index of the thread for this camera, used for frame storage.
     * @throws std::exception If there's an error during stream initialization.
     */
    void startFreeRunStream(std::shared_ptr<Camera> camera,
                            std::atomic<bool> &stopStream,
                            bool saveStream,
                            int threadIndex);
    /**
     * @brief Starts synchronized streaming from multiple cameras using PTP synchronization.
     * @param openCameras List of camera objects to be used for streaming.
     * @param stopStream Atomic boolean to control the streaming process.
     * @param saveStream Boolean to indicate if the streams should be saved to disk.
     * @param acquisitionDelay Optional delay before starting acquisition (useful for setup time).
     * @param displayCallback Optional callback function for displaying frames in a GUI.
     * @note Cameras are started in reverse order for optimal synchronization.
     * @note If no displayCallback is provided, frames are shown in an OpenCV window.
     */
    void startPtpSyncFreeRun(const std::list<std::shared_ptr<Camera>> &openCameras,
                             std::atomic<bool> &stopStream,
                             bool saveStream,
                             std::chrono::milliseconds acquisitionDelay = std::chrono::milliseconds(0),
                             std::function<void(const cv::Mat &)> displayCallback = nullptr);

    /**
     * @brief Stops all streaming threads and cleans up resources.
     * @details This method joins all streaming threads, clears the thread collection,
     *          removes all stored frames from memory, and closes any open OpenCV display windows.
     * @note This is automatically called at the end of startPtpSyncFreeRun but can also be called manually.
     */
    void stopStreaming();

private:
    /**
     * @brief Creates a composite display grid from multiple camera frames.
     * @param frames Vector of OpenCV Mat objects containing camera frames.
     * @return cv::Mat A single composite image containing all camera views.
     * @details This method arranges the input frames in a grid layout based on the number of cameras:
     *          - 1 camera: 1x1 grid
     *          - 2 cameras: 1x2 grid (horizontal arrangement)
     *          - 3-4 cameras: 2x2 grid
     *          - 5-6 cameras: 2x3 grid
     *          Empty frames or missing cameras are shown as black rectangles in the grid.
     *          The composite resolution is fixed at 1280x720 regardless of the input frame sizes.
     * @note If no frames are provided, returns a black image of size 1280x720.
     */
    cv::Mat createComposite(const std::vector<cv::Mat> &frames);

    /**
     * @brief Tracks the number of threads currently running for streaming operations.
     */
    std::atomic<int> startedThreads;

    /**
     * @brief Mutex to ensure thread-safe access to the shared global frame buffer.
     */
    std::mutex globalFrameMutex;

    /**
     * @brief Collection of threads responsible for managing camera streaming operations.
     */
    std::vector<std::thread> threads;

    /**
     * @brief Buffer to store the latest frames captured from each camera.
     * @details Each index corresponds to a specific camera's most recent frame.
     */
    std::vector<cv::Mat> globalFrames;
};

#endif // STREAMMANAGER_HPP
