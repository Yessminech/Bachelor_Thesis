#ifndef STREAMMANAGER_HPP
#define STREAMMANAGER_HPP
#include <opencv2/opencv.hpp>
#include "Camera.hpp"
#include "NetworkManager.hpp"

#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <csignal>

// Todo remove this
#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"

class StreamManager
{
public:
    StreamManager();
    ~StreamManager();
    void stopStreaming();

    cv::Mat createComposite(const std::vector<cv::Mat> &frames);
    void streamFromDevice(std::shared_ptr<Camera> camera, std::atomic<bool>& stopStream, bool saveStream, int threadIndex);
    void startSyncFreeRun(const std::list<std::shared_ptr<Camera>> &openCameras, std::atomic<bool>& stopStream, bool saveStream);

private:
    // std::atomic<bool> stopStream;  // Atomic flag to signal all threads to stopStream.
    std::atomic<int> startedThreads;   // Atomic counter to indicate how many threads have successfully started.
    std::vector<cv::Mat> globalFrames; // Containers for devices and the latest frame for each device.
    std::mutex globalFrameMutex;
    std::vector<std::thread> threads;
};

#endif // STREAMMANAGER_HPP