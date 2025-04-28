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

#ifndef PTPMONITORWORKER_HPP
#define PTPMONITORWORKER_HPP

#include <QObject>
#include <list>
#include <memory>
#include "Camera.hpp"

/**
 * @class PtpMonitorWorker
 * @brief Worker class that monitors the PTP (Precision Time Protocol) synchronization status of multiple cameras.
 *
 * This class runs in its own thread and periodically checks the PTP status of a list of opened cameras.
 * It uses Qt's signal/slot mechanism to report status updates and completion events.
 */
class PtpMonitorWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a new PtpMonitorWorker.
     *
     * @param cameras List of shared pointers to open Camera objects.
     * @param timeout Timeout in milliseconds for the monitoring operation.
     * @param debug Enables or disables debug output.
     */
    PtpMonitorWorker(const std::list<std::shared_ptr<Camera>> &cameras, int timeout, bool debug);

public slots:
    /**
     * @brief Starts the PTP monitoring process.
     *
     * This method runs the PTP status checking logic. It should be called when the thread starts.
     */
    void run();

signals:
    /**
     * @brief Emitted when the monitoring process finishes.
     *
     * @param success True if monitoring completed successfully, false otherwise.
     */
    void finished(bool success);


private:
    /// List of currently opened cameras to monitor.
    std::list<std::shared_ptr<Camera>> openCamerasList;

    /// Timeout for the monitoring process, in milliseconds.
    int monitorPtpStatusTimeoutMs;

    /// Debug flag for verbose logging.
    bool debug;
};

#endif // PTPMONITORWORKER_HPP
