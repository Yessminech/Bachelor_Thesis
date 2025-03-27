#include "PtpMonitorWorker.hpp"
#include <chrono>
#include <thread>

PtpMonitorWorker::PtpMonitorWorker(const std::list<std::shared_ptr<Camera>>& cameras, int timeout, bool debugFlag)
    : openCamerasList(cameras), ptpSyncTimeout(timeout), debug(debugFlag) {}

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

        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(ptpSyncTimeout)) {
            emit statusUpdate("PTP sync timed out.");
            emit finished(false);
            return;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        emit statusUpdate("Waiting for PTP sync...");
    }
}
