#include "GlobalSettings.hpp"

// === Initialization ===
bool debug = true;
std::string defaultCti = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib"; // ToDo
std::atomic<double> g_fpsUpperBound{12.0}; 
std::atomic<double> g_fpsLowerBound{3.0};  
int monitorPtpStatusTimeoutMs = 4; // 4 ms for ptpStatus
int ptpOffsetThresholdNs = 1000; // 1us
int ptpMaxCheck = 20; // 20 checks for ptpOffset
double minExposureTimeMicros = 100000;
double packetSizeB = 9000;           // Jumbo frames (Check hardware max)
double bufferPercent = 10.93;     

// === Function to get/set FPS safely ===
double getGlobalFpsUpperBound()
{
    return g_fpsUpperBound.load();
}

void setGlobalFpsUpperBound(double fps)
{
    g_fpsUpperBound.store(fps);
}

double getGlobalFpsLowerBound()
{
    return g_fpsLowerBound.load();
}

void setGlobalFpsLowerBound(double fps)
{
    g_fpsLowerBound.store(fps);
}
