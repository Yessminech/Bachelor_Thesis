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
    startedThreads = 0;
}

StreamManager::~StreamManager()
{
    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }
    globalFrames.clear();
    cv::destroyAllWindows();
    rcg::System::clearSystems();
}

/// Arrange frames into a composite grid. Supports up to 6 cameras.
cv::Mat StreamManager::createComposite(const std::vector<cv::Mat> &frames)
{
    int n = frames.size();
    int rows = 1, cols = 1;
    if (n == 1) { rows = 1; cols = 1; }
    else if (n == 2) { rows = 1; cols = 2; }
    else if (n <= 4) { rows = 2; cols = 2; }
    else if (n <= 6) { rows = 2; cols = 3; }

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

// Stream from a specific camera
void StreamManager::streamFromDevice(std::shared_ptr<Camera> camera, std::atomic<bool>& stopStream, bool saveStream, int threadIndex)
{
    try
    {
        std::lock_guard<std::mutex> lock(globalFrameMutex);
        globalFrames.push_back(cv::Mat());
       // Normal iteration int index = static_cast<int>(globalFrames.size()) - 1;
        // Ensure the globalFrames vector is large enough
        if (threadIndex >= globalFrames.size()) {
            globalFrames.resize(threadIndex + 1);
        }
        
        if (camera)
        {
            std::cout << "[DEBUG] Launching stream thread for camera " << camera->deviceInfos.id << std::endl;
        }
        else
        {
            std::cerr << RED << "Error: Camera is null" << RESET << std::endl;
            return;
        }

        // Corrected std::thread parameter passing
        //threads.emplace_back([camera, &stopStream, this, index, saveStream]() {
            threads.emplace_back([camera, &stopStream, this, threadIndex, saveStream]() {
            camera->startStreaming(stopStream, this->globalFrameMutex, this->globalFrames, threadIndex, saveStream); // normal iteraion index
        });
        startedThreads++;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Exception during streaming: " << ex.what() << RESET << std::endl;
    }
}

void StreamManager::stopStreaming() {
  
    // Wait for all threads to finish
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    // Clear resources
    threads.clear();
    globalFrames.clear();
    
    // Close any OpenCV windows
    cv::destroyAllWindows();
}

// Start synchronized free-run mode
void StreamManager::startSyncFreeRun(const std::list<std::shared_ptr<Camera>> &openCameras, std::atomic<bool>& stopStream, bool saveStream)
{
    // for (auto it = openCameras.rbegin(); it != openCameras.rend(); ++it)
    // {
    //     const auto &camera = *it;

    //     streamFromDevice(camera, stopStream, saveStream);
    // }
    // Reset the counters
    startedThreads = 0;
    
    // Convert list to vector for easier indexed access
    std::vector<std::shared_ptr<Camera>> cameraVec(openCameras.begin(), openCameras.end());
    
    // Start cameras in the desired order
    for (int i = 0; i < cameraVec.size(); i++) {
        // Pass the index explicitly to control frame placement
        streamFromDevice(cameraVec[i], stopStream, saveStream, i);
        
        // Add a small delay to ensure cameras start in the correct order
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // Wait for all threads to start successfully.
    while (startedThreads.load() < openCameras.size())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << GREEN << "All threads have started successfully." << RESET << std::endl;

    // Create a composite window that scales nicely.
    const std::string compWindowName = "Composite Stream";
    cv::namedWindow(compWindowName, cv::WINDOW_AUTOSIZE);
    const int compWidth = 1280, compHeight = 720;
    cv::resizeWindow(compWindowName, compWidth, compHeight);

    // Composite display loop.
    while (!stopStream)
    {
        std::vector<cv::Mat> framesCopy;
        {
            std::lock_guard<std::mutex> lock(globalFrameMutex);
            framesCopy = globalFrames;
        }
        if (!framesCopy.empty())
        {
            cv::Mat composite = createComposite(framesCopy);
            cv::imshow(compWindowName, composite);
                        // Print a message to remind the user how to exit
                        static bool messageShown = false;
                        if (!messageShown) {
                            std::cout << "Press 'q' on the OpenCV window to exit the stream.\n";
                            messageShown = true;
                        }
                    
        }
    // Process keyboard input - check for specific keys
    int key = cv::waitKey(30);
    if (key >= 0) {
        std::cout << "Key pressed: " << static_cast<char>(key) << " (code: " << key << ")\n";
        
        // Check for common exit keys
        if (key == 27 || key == 'q' || key == 'Q') {  // ESC, q, or Q
            std::cout << "Exit key pressed. Stopping stream...\n";
            stopStream = true;
            break;
        }
    }    }
    stopStreaming();
}

