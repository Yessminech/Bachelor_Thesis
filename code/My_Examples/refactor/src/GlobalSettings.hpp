// In a new header file called GlobalSettings.hpp
#ifndef GLOBAL_SETTINGS_HPP
#define GLOBAL_SETTINGS_HPP

#include <atomic>
#include <mutex>
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>

// === Console Colors (optional) ===
#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"
#define CYAN "\033[36m"

extern bool debug;
extern std::string defaultCti;
extern std::atomic<double> g_fpsUpperBound;
extern std::atomic<double> g_fpsLowerBound;
extern int monitorPtpStatusTimeoutMs;
extern int ptpOffsetThresholdNs;
extern int ptpMaxCheck;
extern double minExposureTimeMicros;
extern double packetSizeB;          
extern double bufferPercent;     

// === Function to get/set FPS safely ===
double getGlobalFpsUpperBound();
void setGlobalFpsUpperBound(double fps);
double getGlobalFpsLowerBound();
void setGlobalFpsLowerBound(double fps);
inline std::string getSessionTimestamp()
{
    static const std::string sessionTimestamp = []()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_time), "%Y%m%d_%H%M%S");
        return ss.str();
    }();
    return sessionTimestamp;
}
#endif