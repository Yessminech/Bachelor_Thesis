#include "StreamManager.hpp"
#include "DeviceManager.hpp"
#include "Camera.hpp"
#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/stream.h>
#include <rc_genicam_api/buffer.h>
#include <rc_genicam_api/image.h>
#include <rc_genicam_api/image_store.h>
#include <rc_genicam_api/nodemap_out.h>
#include <opencv2/opencv.hpp>
#include "rc_genicam_api/pixel_formats.h"

#include <iostream>
#include <atomic>
#include <thread>
#include <csignal>
#include <iomanip>
#include <memory>
#include <string>
#include <stdexcept>
#include <chrono>
#include <sstream>
#include <set>
#include <vector>
#include <mutex>

StreamManager::StreamManager()
{
    // Constructor implementation
    stop_streaming = false;
    startedThreads = 0;
    // threads.clear();
    // globalFrames.clear();
    // globalDevices.clear();
}

StreamManager::~StreamManager()
{
    // Destructor implementation
    stop_streaming = true;
    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }
    cv::destroyAllWindows();
    rcg::System::clearSystems();
}

// Signal handler: when Ctrl+C is pressed.
void StreamManager::signalHandler(int signum)
{
    std::cout << "\n"
              << YELLOW << "Stopping streaming..." << RESET << std::endl;
    stop_streaming = true;
}

/// Arrange frames into a composite grid. Supports up to 6 cameras.
/// The grid layout is determined by the number of streams.
cv::Mat StreamManager::createComposite(const std::vector<cv::Mat> &frames)
{
    int n = frames.size();
    int rows = 1, cols = 1;
    if (n == 1)
    {
        rows = 1;
        cols = 1;
    }
    else if (n == 2)
    {
        rows = 1;
        cols = 2;
    }
    else if (n <= 4)
    {
        rows = 2;
        cols = 2;
    }
    else if (n <= 6)
    {
        rows = 2;
        cols = 3;
    }
    int compWidth = 1280, compHeight = 720;
    int cellWidth = compWidth / cols;
    int cellHeight = compHeight / rows;

    std::vector<cv::Mat> rowImages;
    for (int r = 0; r < rows; r++)
    {
        std::vector<cv::Mat> cellImages;
        for (int c = 0; c < cols; c++)
        {
            int idx = r * cols + c;
            cv::Mat cell;
            if (idx < n && !frames[idx].empty())
            {
                cv::resize(frames[idx], cell, cv::Size(cellWidth, cellHeight));
            }
            else
            {
                cell = cv::Mat::zeros(cellHeight, cellWidth, CV_8UC3);
            }
            cellImages.push_back(cell);
        }
        cv::Mat rowConcat;
        cv::hconcat(cellImages, rowConcat);
        rowImages.push_back(rowConcat);
    }
    cv::Mat composite;
    cv::vconcat(rowImages, composite);
    return composite;
}

// ToDo Save images

/// Streams from a specific device identified by deviceID.
void StreamManager::streamFromDevice(std::shared_ptr<Camera> camera, bool save_footage)
{
    try
    {
        std::lock_guard<std::mutex> lock(globalFrameMutex);
        globalFrames.push_back(cv::Mat());
        int index = static_cast<int>(globalFrames.size()) - 1;
        // if (camera->debug)
        // std::cout << "[Camera::debug] Launching stream thread for camera " << camera->getID() << std::endl;
        threads.emplace_back(&Camera::startStreaming, camera.get(), false, std::ref(globalFrameMutex), std::ref(globalFrames), index, save_footage);
        // Join all threads // ToDo Why this
    }
    catch (const std::exception &ex)
    {
        std::cout << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
    }
}

void StreamManager::startSyncFreeRun(const std::list<std::shared_ptr<Camera>> &OpenCameras, bool save_footage)
{
    for (const auto &camera : OpenCameras)
    {
        streamFromDevice(camera, save_footage);
    }
    return;
}