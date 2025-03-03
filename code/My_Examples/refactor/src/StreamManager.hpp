std::atomic<bool> stop_signal(false);
std::atomic<bool> stop_streaming(false);// Global atomic flag to signal all threads to stop.
std::atomic<int> startedThreads(0);// Global atomic counter to indicate how many threads have successfully started.
// Global containers for devices and the latest frame for each device.
std::vector<cv::Mat> globalFrames;
std::mutex globalFrameMutex;
std::vector<std::thread> threads;

void signalHandler(int signum);
cv::Mat createComposite(const std::vector<cv::Mat> &frames);
void streamFromAllDevices();
void streamFromDevice(const std::string &deviceID);
bool startStreaming(std::shared_ptr<rcg::Device> device);
void stopStreaming(std::shared_ptr<rcg::Device> device);
void processRawFrame(const cv::Mat &rawFrame, cv::Mat &outputFrame, uint64_t pixelFormat);
void startSyncFreeRun(const std::shared_ptr<rcg::Device> &device, int index);