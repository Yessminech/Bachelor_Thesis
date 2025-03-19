// In a new header file called GlobalSettings.hpp
#ifndef GLOBAL_SETTINGS_HPP
#define GLOBAL_SETTINGS_HPP

#include <atomic>
#include <mutex>

// Thread-safe global FPS value
extern std::atomic<double> g_fpsUpperBound;
extern std::atomic<double> g_fpsLowerBound;

// Function to get/set FPS safely
double getGlobalFpsUpperBound();
void setGlobalFpsUpperBound(double fps);
double getGlobalFpsLowerBound();
void setGlobalFpsLowerBound(double fps);

#endif