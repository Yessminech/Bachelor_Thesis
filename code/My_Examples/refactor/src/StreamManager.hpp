#ifndef STREAMMANAGER_HPP
#define STREAMMANAGER_HPP

#include <opencv2/opencv.hpp>

#include "Camera.hpp"
#include "NetworkManager.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <list>

class StreamManager
{
public:
    // === Constructor / Destructor ===
    StreamManager();
    ~StreamManager();

    // === Streaming ===
    void startFreeRunStream(std::shared_ptr<Camera> camera,
                            std::atomic<bool> &stopStream,
                            bool saveStream,
                            int threadIndex);

    void startPtpSyncFreeRun(const std::list<std::shared_ptr<Camera>> &openCameras,
                          std::atomic<bool> &stopStream,
                          bool saveStream,
                          std::chrono::milliseconds acquisitionDelay = std::chrono::milliseconds(0),
                          std::function<void(const cv::Mat &)> displayCallback = nullptr);

    void stopStreaming();

private:
    // === Utility ===
    cv::Mat createComposite(const std::vector<cv::Mat> &frames);

    // === Private Members ===
    // === Threading & Synchronization ===
    std::atomic<int> startedThreads;  // Number of started threads
    std::mutex globalFrameMutex;      // Mutex for shared frame buffer
    std::vector<std::thread> threads; // Vector of streaming threads

    // === Frame Data ===
    std::vector<cv::Mat> globalFrames; // Holds latest frames from each camera
};

#endif // STREAMMANAGER_HPP
