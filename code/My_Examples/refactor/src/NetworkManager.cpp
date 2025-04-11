#include "Camera.hpp"
#include "NetworkManager.hpp"
#include "DeviceManager.hpp"
#include "GlobalSettings.hpp"

#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>
#include <GenApi/GenApi.h>

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <iomanip>
#include <signal.h>
#include <atomic>
#include <set>
#include <fstream>   
#include <algorithm> 
#include <deque>    
#include <random>
#include <filesystem>
#include "qcustomplot.h"
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QMap>
#include <QStringList>
#include <QDebug>
#include <QDir>
#include <QRegularExpression>

double fpsUpperBound = getGlobalFpsUpperBound();
double fpsLowerBound = getGlobalFpsLowerBound();

/**************************************************************************************/
/***********************         Constructor / Destructor     *************************/
/**************************************************************************************/

NetworkManager::NetworkManager(){};

NetworkManager::~NetworkManager() {}

/*************************************************************************/
/***********************         PTP Control     *************************/
/*************************************************************************/

void NetworkManager::enablePtp(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    for (const auto &camera : openCameras)
    {
        camera->setPtpConfig(true);
    }
}

void NetworkManager::disablePtp(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    for (const auto &camera : openCameras)
    {
        camera->setPtpConfig(false);
    }
}

//ToDo correct units with ai 
void NetworkManager::monitorPtpStatus(const std::list<std::shared_ptr<Camera>> &openCamerasList)
{
    auto start_time = std::chrono::steady_clock::now();
    bool synchronized = false;
    int numInit = 0;
    int numMaster = 0;
    int numSlave = 0;

    std::cout << "[DEBUG] Starting PTP synchronization monitoring..." << std::endl;

    // Continue until synchronized, timed out
    while (!synchronized && 
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count() < monitorPtpStatusTimeoutMs)
    {

        // Print current status
        std::cout << "[DEBUG] Current status: " << numInit << " initializing, "
                  << numMaster << " masters, " << numSlave << " slaves" << std::endl;

        // Reset counters for each check
        numInit = 0;
        numMaster = 0;
        numSlave = 0;

        // Check all cameras
        for (auto &camera : openCamerasList)
        {
            try
            {
                camera->getPtpConfig();
                PtpConfig ptpConfig = camera->ptpConfig;

                // Update status counters
                if (ptpConfig.status == "Master")
                {
                    numMaster++;
                    masterClockId = camera->deviceInfos.id; // Store the master clock ID
                }
                else if (ptpConfig.status == "Slave")
                {
                    numSlave++;
                }
                else
                {
                    numInit++;
                }
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[DEBUG] Error checking PTP status for camera "
                          << camera->deviceInfos.id << ": " << ex.what() << RESET << std::endl;
                numInit++; // Count as initializing if there's an error
            }
        }
        // Check if we have a valid synchronization state:
        // - One master and all others are slaves (normal case)
        // - All slaves and master is external (external clock source)
        if (numMaster == 1 && numSlave == openCamerasList.size() - 1 && numInit == 0)
        {
            std::cout << GREEN << "[DEBUG] PTP synchronization successful!" << RESET << std::endl;
            synchronized = true;
        }

        // Check for timeout
        if (std::chrono::steady_clock::now() - start_time > std::chrono::milliseconds(monitorPtpStatusTimeoutMs))
        {
            std::cerr << RED << "[DEBUG] PTP synchronization timed out after " 
                      << (monitorPtpStatusTimeoutMs / 1000.0) // Convert to seconds for display
                      << " seconds. Current status: " << numInit << " initializing, "
                      << numMaster << " masters, " << numSlave << " slaves" << RESET << std::endl;
            break;
        }

        // If not synchronized yet, wait before checking again
        if (!synchronized)
        {
            if (debug)
            {
                std::cout << YELLOW << "[DEBUG] Waiting for PTP synchronization..." << RESET << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }

    // Print final synchronization information
    if (synchronized)
    {
        if (debug)
        {
            std::cout << GREEN << "[DEBUG] PTP synchronized with " << numSlave
                      << " slave cameras and " << numMaster << " master" << RESET << std::endl;
        }
    }
}

void NetworkManager::monitorPtpOffset(const std::list<std::shared_ptr<Camera>> &openCamerasList)
{
    bool allSynced = false;
    int checkCount = 0;

    if (debug)
    {
        std::cout << YELLOW << "[DEBUG] Monitoring PTP offset synchronization..." << RESET << std::endl;
    }

    std::shared_ptr<Camera> masterCamera = nullptr;
    for (auto &cam : openCamerasList)
    {
        cam->getPtpConfig();
        if (cam->ptpConfig.status == "Master")
        {
            masterCamera = cam;
            masterClockId = cam->deviceInfos.id;
            break;
        }
    }

    if (!masterCamera)
    {
        std::cerr << RED << "[DEBUG] Error: No master camera found in the camera list" << RESET << std::endl;
        return;
    }
    
    std::unordered_map<std::string, std::deque<CameraSample>> offsetWindowHistory; // For stability check
    std::unordered_map<std::string, std::vector<CameraSample>> offsetLogHistory;   // For logging to CSV
        
    while (!allSynced && checkCount < ptpMaxCheck )
    {
        allSynced = true;
        checkCount++;

        if (debug)
        {
            std::cout << YELLOW << "[DEBUG] PTP offset check #" << checkCount << RESET << std::endl;
        }

        for (auto &camera : openCamerasList)
        {
            try
            {
                camera->getTimestamps();
                camera->getPtpConfig();
        
                std::string camId = camera->deviceInfos.id;
                int64_t offset = (camId == masterClockId) ? 0 : std::abs(camera->ptpConfig.offsetFromMaster);
                uint64_t timestamp = camera->ptpConfig.timestamp_ns;
                offsetLogHistory[camId].push_back(CameraSample{offset, timestamp});
                auto &window = offsetWindowHistory[camId];
                if (window.size() >= timeWindowSize)
                window.pop_front();
                window.push_back(CameraSample{offset, timestamp});
        
                // Only evaluate stability for non-master cameras
                if (camId != masterClockId && window.size() == timeWindowSize)
                {
                    bool inWindow = std::all_of(window.begin(), window.end(),
                        [&](const CameraSample &sample) { return sample.offset_ns <= ptpOffsetThresholdNs; });
        
                    if (!inWindow)
                    {
                        allSynced = false;
                        if (debug)
                            std::cout << RED << "[DEBUG] Camera " << camId
                                      << " not stable within time window." << RESET << std::endl;
                    }
                    else if (debug)
                    {
                        std::cout << GREEN << "[DEBUG] Camera " << camId
                                  << " stable within time window." << RESET << std::endl;
                    }
                }
                else if (camId != masterClockId)
                {
                    allSynced = false;
                    if (debug)
                    {
                        std::cout << YELLOW << "[DEBUG] Camera " << camId
                                  << " collecting samples (" << window.size() << "/" << timeWindowSize << ")" << RESET << std::endl;
                    }
                }
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[DEBUG] Error checking PTP offset for camera "
                          << camera->deviceInfos.id << ": " << ex.what() << RESET << std::endl;
                allSynced = false;
            }
        }
        if (!allSynced && checkCount < ptpMaxCheck)
        {
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }
    std::unordered_map<std::string, std::deque<CameraSample>> converted;

    for (const auto& [key, vec] : offsetLogHistory)
    {
        converted[key] = std::deque<CameraSample>(vec.begin(), vec.end());
    }
    
    logOffsetHistoryToCSV(converted);
    if (debug)
    {
        if (allSynced)
        {
            std::cout << GREEN << "[DEBUG] All cameras are stable and synchronized within the time window." << RESET << std::endl;
            for (auto &camera : openCamerasList)
            {
                camera->getTimestamps();
                printPtpConfig(camera);
            }
        }
        else
        {
            std::cerr << RED << "[DEBUG] Synchronization failed: not all cameras were stable within threshold over "
                      << timeWindowSize << " consecutive samples." << RESET << std::endl;
        }
    }
}

void NetworkManager::setOffsetfromMaster(std::shared_ptr<Camera> masterCamera, std::shared_ptr<Camera> camera)
{
    // Get the master camera's timestamp
    masterCamera->getTimestamps();
    uint64_t masterTimestamp = masterCamera->ptpConfig.timestamp_ns;

    // Get the camera's timestamp
    camera->getTimestamps();
    uint64_t cameraTimestamp = camera->ptpConfig.timestamp_ns;

    // Calculate the offset
    int64_t offset = masterTimestamp- cameraTimestamp;

    // Set the offset
    try
    {
        if (camera->ptpConfig.offsetFromMaster == 0 && camera->deviceInfos.id != masterClockId)
        {
            auto nodemap = camera->nodemap;
            camera->ptpConfig.offsetFromMaster = offset;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "[DEBUG] Error setting offset for camera " << camera->deviceInfos.id << ": " << ex.what() << RESET << std::endl;
    }
}

/*************************************************************************************/
/***********************         Bandwidth Configuration     *************************/
/*************************************************************************************/

void NetworkManager::configureBandwidth(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    double packetDelayNs;
    for (auto it = openCameras.rbegin(); it != openCameras.rend(); ++it)
    {
        const auto &camera = *it;
        // double deviceLinkSpeedBps = camera->networkConfig.deviceLinkSpeedBps;
        double deviceLinkSpeedBps = 125000000;               //  125000000 Lucid and 100000000 for Basler
        camera->setPacketSizeB(packetSizeB);
        double camIndex = std::distance(openCameras.begin(), std::find(openCameras.begin(), openCameras.end(), camera));
        double numCams = openCameras.size();
        camera->setBandwidthDelays(camera, camIndex, numCams, packetSizeB, deviceLinkSpeedBps, bufferPercent);
        packetDelayNs = camera->networkConfig.packetDelayNs;
    }
    calculateMaxFps(openCameras, packetDelayNs);
    setExposureAndFps(openCameras);
}

void NetworkManager::calculateMaxFps(const std::list<std::shared_ptr<Camera>> &openCameras, double packetDelay)
{
    double currentMinFps = fpsUpperBound;
    for (const auto &camera : openCameras)
    {
        double deviceLinkSpeedBps = camera->networkConfig.deviceLinkSpeedBps;
        double calculatedFps = camera->calculateFps(deviceLinkSpeedBps, packetSizeB);
        double clampedFps = std::max(fpsLowerBound, std::min(calculatedFps, fpsUpperBound));
        if (clampedFps < currentMinFps)
        {
            currentMinFps = clampedFps;
        }
    }
    setGlobalFpsUpperBound(currentMinFps);
}

void NetworkManager::getMinimumExposure(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    for (const auto &camera : openCameras)
    {
        camera->setExposureTime(minExposureTimeMicros);
    }
    double currentMinExposure = minExposureTimeMicros;
    for (const auto &camera : openCameras)
    {
        double exposure = camera->getExposureTime();
        if (exposure < currentMinExposure)
        {
            currentMinExposure = exposure;
        }
    }
    minExposureTimeMicros = currentMinExposure;
}

void NetworkManager::setExposureAndFps(const std::list<std::shared_ptr<Camera>> &openCameras)
{
    fpsUpperBound = getGlobalFpsUpperBound();
    getMinimumExposure(openCameras);
    if (debug)
    {
        std::cout << YELLOW << "[DEBUG] Setting exposure to " << minExposureTimeMicros << " and fps to: " << fpsUpperBound << " across all cameras." << RESET << std::endl;
    }
    for (const auto &camera : openCameras)
    {
        camera->setFps(fpsUpperBound);
        if (minExposureTimeMicros <= 1000000 / fpsUpperBound)
        {
            camera->setExposureTime(minExposureTimeMicros); // time in microseconds
        }
        else
        {
            std::cout << "Exposure Time" << minExposureTimeMicros << "exceeds maximal exposure: " << 1000000 / fpsUpperBound << std::endl;
            camera->setExposureTime(1000000 / fpsUpperBound); // time in microseconds
        }
    }
}

void NetworkManager::configurePtpSyncFreeRun(const std::list<std::shared_ptr<Camera>> &openCamerasList)
{
    enablePtp(openCamerasList); // Should be before configureBandwidth
    configureBandwidth(openCamerasList);
    monitorPtpStatus(openCamerasList);
    monitorPtpOffset(openCamerasList);
    plotOffsets();
    return;
}

/***************************************************************************************/
/***********************         Logging and Visualization     *************************/
/***************************************************************************************/

void NetworkManager::printPtpConfig(std::shared_ptr<Camera> camera)
{
    try
    {
        PtpConfig ptpConfig = camera->ptpConfig;
        std::cout << GREEN << "Camera ID:                  " << camera->deviceInfos.id << RESET << std::endl;
        std::cout << "PTP Enabled:                " << (ptpConfig.enabled ? "Yes" : "No") << std::endl;
        std::cout << "PTP Status:                 " << ptpConfig.status << std::endl;
        std::cout << "PTP Offset From Master:     " << ptpConfig.offsetFromMaster << " ns" << std::endl;
        std::cout << "Timestamp (ns):                  " << ptpConfig.timestamp_ns << " ns" << std::endl;
        std::cout << "Timestamp (s):        " << ptpConfig.timestamp_s << " s" << std::endl;
        std::cout << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get timestamp: " << ex.what() << RESET << std::endl;
    }
}

void NetworkManager::logOffsetHistoryToCSV(
    const std::unordered_map<std::string, std::deque<CameraSample>> &offsetHistory)
{
    // Directories and filename
    const std::string outputDir = "./output";
    const std::string offsetDir = outputDir + "/offset";
    const std::string timestamp = getSessionTimestamp();
    const std::string filename = offsetDir + "/ptp_offset_history_" + timestamp + ".csv";

    // Ensure directories exist
    std::filesystem::create_directories(offsetDir);

    std::ofstream outFile(filename);
    if (!outFile.is_open())
    {
        std::cerr << RED << "[DEBUG] Failed to open CSV file for writing: " << filename << RESET << std::endl;
        return;
    }

    // Header
    outFile << "Sample";
    for (const auto &[camId, _] : offsetHistory)
    {
        outFile << "," << camId << "_timestamp_ns," << camId << "_offset_ns";
    }
    outFile << "\n";

    // Determine max length of history
    size_t maxLength = 0;
    for (const auto &[_, history] : offsetHistory)
        maxLength = std::max(maxLength, history.size());

    // Write rows
    for (size_t i = 0; i < maxLength; ++i)
    {
        outFile << i;
        for (const auto &[_, history] : offsetHistory)
        {
            if (i < history.size())
            {
                outFile << "," << history[i].timestamp_ns << "," << history[i].offset_ns;
            }
            else
            {
                outFile << ",,";
            }
        }
        outFile << "\n";
    }

    outFile.close();
    std::cout << CYAN << "[DEBUG] Offset + timestamp history written to " << filename << RESET << std::endl;
}

uint32_t ipToDecimal(std::string ip)
{
    std::vector<std::string> octets;
    std::stringstream ss(ip);
    std::string octet;
    while (std::getline(ss, octet, '.'))
    {
        octets.push_back(octet);
    }
    uint32_t decimal = (std::stoi(octets[0]) << 24) + (std::stoi(octets[1]) << 16) + (std::stoi(octets[2]) << 8) + std::stoi(octets[3]);
    return decimal;
}

void NetworkManager::plotOffsets(double ptpThreshold)
{
    const QString offsetDir = "./output/offset";
    const QString plotsDir = "./output/plots";

    std::filesystem::create_directories(plotsDir.toStdString());

    // Find latest matching CSV: ptp_offset_history_*.csv
    QDir dir(offsetDir);
    QStringList csvFiles = dir.entryList(QStringList() << "ptp_offset_history_*.csv", QDir::Files, QDir::Name);

    if (csvFiles.isEmpty()) {
        qDebug() << "[ERROR] No ptp_offset_history_*.csv files found in:" << offsetDir;
        return;
    }

    QString latestCsv = csvFiles.last(); // Sorted, so last one is newest
    QString csvPath = offsetDir + "/" + latestCsv;

    // Extract timestamp from filename (for consistent plot naming)
    QString sessionTimestamp;
    QRegularExpression re("ptp_offset_history_(\\d{8}_\\d{6})\\.csv");
    QRegularExpressionMatch match = re.match(latestCsv);
    if (match.hasMatch()) {
        sessionTimestamp = match.captured(1);
    } else {
        sessionTimestamp = QString::fromStdString(getSessionTimestamp());
    }

    // Now parse the CSV and plot
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[ERROR] Failed to open CSV file:" << csvPath;
        return;
    }

    QTextStream in(&file);
    QString headerLine = in.readLine();
    QStringList headers = headerLine.split(',');

    QVector<double> samples;
    QMap<QString, QVector<double>> offsetData;
    QStringList offsetHeaders;

    for (const QString &h : headers) {
        if (h.endsWith("_offset_ns")) {
            offsetHeaders.append(h);
            offsetData[h] = QVector<double>();
        }
    }

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList values = line.split(',');
        if (values.size() < headers.size()) continue;

        samples.append(values[0].toDouble());

        for (const QString &offsetHeader : offsetHeaders) {
            int idx = headers.indexOf(offsetHeader);
            if (idx < values.size()) {
                offsetData[offsetHeader].append(values[idx].toDouble());
            } else {
                offsetData[offsetHeader].append(0.0);
            }
        }
    }

    QCustomPlot plot;
    plot.legend->setVisible(true);
    plot.legend->setBrush(QBrush(QColor(255, 255, 255, 230)));

    int colorIndex = 0;
    for (const QString &offsetHeader : offsetHeaders) {
        plot.addGraph();
        QString label = offsetHeader;
        label.replace("_offset_ns", "");
        plot.graph()->setName(label);

        plot.graph()->setData(samples, offsetData[offsetHeader]);
        plot.graph()->setPen(QPen(QColor::fromHsv((colorIndex * 80) % 360, 255, 200), 2));
        colorIndex++;
    }

    // Add threshold line
    QCPItemStraightLine *thresholdLine = new QCPItemStraightLine(&plot);
    thresholdLine->point1->setCoords(0, ptpThreshold);
    thresholdLine->point2->setCoords(1, ptpThreshold);
    thresholdLine->setPen(QPen(Qt::red, 1, Qt::DashLine));

    QCPItemText *thresholdLabel = new QCPItemText(&plot);
    thresholdLabel->position->setCoords(samples.last(), ptpThreshold + 20);
    thresholdLabel->setText("Threshold");
    thresholdLabel->setColor(Qt::red);
    thresholdLabel->setFont(QFont("Sans", 9));

    plot.xAxis->setLabel("Sample Index");
    plot.yAxis->setLabel("Offset from Master (ns)");
    plot.rescaleAxes();

    // Save the plot
    QString plotPath = plotsDir + QString("/ptp_offsets_%1.png").arg(sessionTimestamp);
    int width = 1200;
    int height = 600;
    plot.resize(width, height);
    if (plot.toPixmap(width, height).save(plotPath)) {
        qDebug() << "[INFO] Offset plot saved to" << plotPath;
    } else {
        qDebug() << "[ERROR] Failed to save plot to" << plotPath;
    }
}