// In a new header file called GlobalSettings.hpp
#ifndef GLOBAL_SETTINGS_HPP
#define GLOBAL_SETTINGS_HPP

#include <atomic>
#include <mutex>
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>
// Thread-safe global FPS value
extern std::atomic<double> g_fpsUpperBound;
extern std::atomic<double> g_fpsLowerBound;

// Function to get/set FPS safely
double getGlobalFpsUpperBound();
void setGlobalFpsUpperBound(double fps);
double getGlobalFpsLowerBound();
void setGlobalFpsLowerBound(double fps);
inline std::string getSessionTimestamp()
{
    static const std::string sessionTimestamp = []() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_time), "%Y%m%d_%H%M%S");
        return ss.str();
    }();
    return sessionTimestamp;
}
#endif