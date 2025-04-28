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

#include "PtpMonitorWorker.hpp"
#include <chrono>
#include <thread>

PtpMonitorWorker::PtpMonitorWorker(const std::list<std::shared_ptr<Camera>>& cameras, int timeout, bool debugFlag)
    : openCamerasList(cameras), monitorPtpStatusTimeoutMs(timeout), debug(debugFlag) {}

void PtpMonitorWorker::run() {
    auto start_time = std::chrono::steady_clock::now();
    bool synchronized = false;

    int numInit = 0, numMaster = 0, numSlave = 0;

    emit statusUpdate("Starting PTP sync...");

    while (!synchronized) {
        numInit = numMaster = numSlave = 0;

        for (auto &camera : openCamerasList) {
            try {
                camera->getPtpConfig();
                PtpConfig ptpConfig = camera->ptpConfig;

                if (ptpConfig.status == "Master") {
                    numMaster++;
                } else if (ptpConfig.status == "Slave") {
                    numSlave++;
                } else {
                    numInit++;
                }
            } catch (...) {
                numInit++;
            }
        }

        if (numMaster == 1 && numSlave == openCamerasList.size() - 1 && numInit == 0) {
            synchronized = true;
            emit statusUpdate("PTP synchronization successful.");
            emit finished(true);
            return;
        }

        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(monitorPtpStatusTimeoutMs)) {
            emit statusUpdate("PTP sync timed out.");
            emit finished(false);
            return;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        emit statusUpdate("Waiting for PTP sync...");
    }
}
