// In GlobalSettings.cpp
#include "GlobalSettings.hpp"

// Initialize with default value
std::atomic<double> g_fpsUpperBound{12.0}; // ToDo
std::atomic<double> g_fpsLowerBound{3.0}; // ToDo
 
double getGlobalFpsUpperBound() {
    return g_fpsUpperBound.load();
}

void setGlobalFpsUpperBound(double fps) {
    g_fpsUpperBound.store(fps);
}

double getGlobalFpsLowerBound() {
    g_fpsLowerBound.load();
}

void setGlobalFpsLowerBound(double fps) {
    g_fpsLowerBound.store(fps);
}

