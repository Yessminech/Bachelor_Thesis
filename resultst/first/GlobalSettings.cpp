/**
 * @brief Library component of the TU Berlin industrial automation framework
 *
 * Copyright (c) 2025  
 * TU Berlin, Institut für Werkzeugmaschinen und Fabrikbetrieb  
 * Fachgebiet Industrielle Automatisierungstechnik  
 * Author: Yessmine Chabchoub  
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.  
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.  
 * 3. Neither the name of the copyright holder nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without specific prior written permission.
 *
 * @note
 * DISCLAIMER  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "GlobalSettings.hpp"

#include <csignal>
#include <iostream>

// === Initialization ===
bool debug = true;
std::string defaultCti = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib"; // ToDo
std::atomic<double> g_fpsUpperBound{12.0}; 
std::atomic<double> g_fpsLowerBound{3.0};  
int monitorPtpStatusTimeoutMs = 20; // 20 ms for ptpStatus
int ptpOffsetThresholdNs = 1000; // 1us
int ptpMaxCheck = 20; // 20 checks for ptpOffset
double minExposureTimeMicros = 10000;
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
