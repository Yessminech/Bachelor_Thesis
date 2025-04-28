/**
 * @brief Global settings and utilities for the TU Berlin industrial automation framework
 *
 * Copyright (c) 2025
 * TU Berlin, Institut für Werkzeugmaschinen und Fabrikbetrieb
 * Fachgebiet Industrielle Automatisierungstechnik
 * Author: Yessmine Chabchoub
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written permission.
 *
 * @note
 * DISCLAIMER
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DAMAGES ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE.
 */

#ifndef GLOBAL_SETTINGS_HPP
#define GLOBAL_SETTINGS_HPP

#include <atomic>
#include <mutex>
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>

/**
 * Console color codes for terminal output
 */
#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"
#define CYAN "\033[36m"

/**
 * @brief Enables debug output when true
 * @details Controls the verbosity of console output and logging
 */
extern bool debug;

/**
 * @brief Default CTI (Camera Technology Interface) file path
 * @details Path to the GenTL producer used for camera discovery
 */
extern std::string defaultCti;

/**
 * @brief Upper bound for frame rate in frames per second
 * @details Thread-safe atomic variable that limits maximum acquisition speed
 * @note Values typically range from 1.0 to 15.0 fps
 */
extern std::atomic<double> g_fpsUpperBound;

/**
 * @brief Lower bound for frame rate in frames per second
 * @details Thread-safe atomic variable that enforces minimum acquisition speed
 * @note Used for diagnostics and performance monitoring
 */
extern std::atomic<double> g_fpsLowerBound;

/**
 * @brief Timeout for PTP status synchronization in milliseconds
 * @details Maximum time to wait for cameras to synchronize via PTP protocol
 * @note A typical value is 20ms
 */
extern int monitorPtpStatusTimeoutMs;

/**
 * @brief Maximum acceptable PTP offset between master and slave in nanoseconds
 * @details Cameras with offset exceeding this threshold are considered out of sync
 * @note Typical value for industrial cameras is between 1000-10000 ns
 */
extern int ptpOffsetThresholdNs;

/**
 * @brief Maximum number of PTP synchronization verification attempts
 * @details Controls how many times the system checks PTP status before giving up
 */
extern int ptpMaxCheck;

/**
 * @brief Minimum exposure time in microseconds
 * @details Sets the lower limit for camera exposure to insure value is same across all cameras
 * @note Exact minimum depends on cameras configuration
 */
extern double minExposureTimeMicros;

/**
 * @brief Network packet size in bytes
 * @details Used for GigE camera configuration and bandwidth calculations
 * @note Typical values: 9000 (jumbo frames)
 */
extern double packetSizeB;

/**
 * @brief Memory buffer allocation percentage
 * @details Controls how much memory to allocate for frame buffers
 * @note Typical value: 10.93-20.0
 */
extern double bufferPercent;

/**
 * @brief Gets the global FPS upper bound.
 */
double getGlobalFpsUpperBound();

/**
 * @brief Sets the global FPS upper bound.
 */
void setGlobalFpsUpperBound(double fps);

/**
 * @brief Gets the global FPS lower bound.
 */
double getGlobalFpsLowerBound();

/**
 * @brief Sets the global FPS lower bound.
 */
void setGlobalFpsLowerBound(double fps);

/**
 * @brief Returns the session timestamp as a string.
 */
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

#endif // GLOBAL_SETTINGS_HPP
