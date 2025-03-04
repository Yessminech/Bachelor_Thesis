#ifndef STREAMMANAGER_HPP
#define STREAMMANAGER_HPP

#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <opencv2/opencv.hpp>

// Todo remove this
#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"

class StreamManager {
public:
    StreamManager();
    ~StreamManager();

    void signalHandler(int signum);
    cv::Mat createComposite(const std::vector<cv::Mat> &frames);
    void streamFromDevice(const std::string &deviceID);
    void startSyncFreeRun();
    void stopSyncFreeRun(); //ToDo
private:
    std::atomic<bool> stop_streaming; // Atomic flag to signal all threads to stop.
    std::atomic<int> startedThreads; // Atomic counter to indicate how many threads have successfully started.
    std::vector<cv::Mat> globalFrames; // Containers for devices and the latest frame for each device.
    std::mutex globalFrameMutex;
    std::vector<std::thread> threads;
};

#endif // STREAMMANAGER_HPP