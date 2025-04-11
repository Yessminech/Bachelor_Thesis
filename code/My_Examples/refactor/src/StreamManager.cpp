#include "StreamManager.hpp"
#include "DeviceManager.hpp"
#include "GlobalSettings.hpp"
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

/**************************************************************************************/
/***********************         Constructor / Destructor     *************************/
/**************************************************************************************/

StreamManager::StreamManager()
    : startedThreads(0)
{
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

/***********************************************************************/
/***********************         Streaming     *************************/
/***********************************************************************/

// Stream from a specific camera
void StreamManager::startFreeRunStream(std::shared_ptr<Camera> camera, std::atomic<bool> &stopStream, bool saveStream, int threadIndex)
{
    try
    {
        std::lock_guard<std::mutex> lock(globalFrameMutex);
        globalFrames.push_back(cv::Mat());
        camera->setFreeRunMode();
        // Ensure the globalFrames vector is large enough
        if (threadIndex >= globalFrames.size())
        {
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

        threads.emplace_back([camera, &stopStream, this, threadIndex, saveStream]()
                             {
                                 camera->startStreaming(stopStream, this->globalFrameMutex, this->globalFrames, threadIndex, saveStream); // normal iteraion index
                             });
        startedThreads++;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Exception during streaming: " << ex.what() << RESET << std::endl;
    }
}

// Start synchronized free-run mode
void StreamManager::startPtpSyncFreeRun(
    const std::list<std::shared_ptr<Camera>> &openCameras,
    std::atomic<bool> &stopStream,
    bool saveStream,
    std::chrono::milliseconds acquisitionDelay,
    std::function<void(const cv::Mat &)> displayCallback)
{   
    std::cout << "Starting synchronized free-run stream with " << openCameras.size() << " cameras." << std::endl;
    // Optional delay before starting acquisition
    if (acquisitionDelay.count() > 0)
    {
        std::cout << YELLOW << "Waiting " << acquisitionDelay.count() << " ms before starting acquisition..." << RESET << std::endl;
        std::this_thread::sleep_for(acquisitionDelay);
    }

    // Reset the counters
    startedThreads = 0;

    // Convert list to vector for easier indexed access
    std::vector<std::shared_ptr<Camera>> cameraVec(openCameras.begin(), openCameras.end());

    // Start cameras in reverse order (last to first)
    for (int i = cameraVec.size() - 1; i >= 0; i--)
    {
        startFreeRunStream(cameraVec[i], stopStream, saveStream, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay between starts
    }

    // Wait for all threads to start
    while (startedThreads.load() < openCameras.size())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << GREEN << "All threads have started successfully." << RESET << std::endl;

    // Composite window creation
    const std::string compWindowName = "Composite Stream";
    const int compWidth = 1280, compHeight = 720;
    // Only show OpenCV window if no GUI callback is given
    if (!displayCallback)
    {
        cv::namedWindow(compWindowName, cv::WINDOW_AUTOSIZE);
        cv::resizeWindow(compWindowName, compWidth, compHeight);
    }
    // Composite display loop
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
            if (displayCallback)
            {
                displayCallback(composite); // Send to GUI
            }
            else
            {
                cv::imshow(compWindowName, composite); // Default OpenCV view
            }

            static bool messageShown = false;
            if (!messageShown)
            {
                std::cout << "Press 'q' on the OpenCV window to exit the stream.\n";
                messageShown = true;
            }
        }

        int key = cv::waitKey(30);
        if (key >= 0)
        {
            std::cout << "Key pressed: " << static_cast<char>(key) << " (code: " << key << ")\n";
            if (key == 27 || key == 'q' || key == 'Q')
            {
                std::cout << "Exit key pressed. Stopping stream...\n";
                stopStream = true;
                break;
            }
        }
    }

    stopStreaming();
}

void StreamManager::stopStreaming()
{
    // Wait for all threads to finish
    for (auto &t : threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    // Clear resources
    threads.clear();
    globalFrames.clear();

    // Close any OpenCV windows
    cv::destroyAllWindows();
}
/***********************         Private Methods     *************************/

/*********************************************************************/
/***********************         Utility     *************************/
/*********************************************************************/

/// Arrange frames into a composite grid. Supports up to 6 cameras.
cv::Mat StreamManager::createComposite(const std::vector<cv::Mat> &frames)
{
    if (frames.empty()) 
{
        return cv::Mat::zeros(720, 1280, CV_8UC3);
    }

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