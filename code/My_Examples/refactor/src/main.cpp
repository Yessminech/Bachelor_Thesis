#include "DeviceManager.hpp"
#include "NetworkManager.hpp"
#include "StreamManager.hpp"

#include "Camera.hpp"
#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <sstream>
#include <set>
#include <arpa/inet.h>
#include <iomanip>
#include <regex>
#include <csignal>

std::atomic<bool> stopStream(false);
DeviceManager deviceManager;
NetworkManager networkManager = NetworkManager(true);
StreamManager streamManager;

void handleSignal(int signal)
{
    if (signal == SIGINT)
    {
        std::cout << "\nReceived Ctrl+C, stopping streams and cleanup...\n";
        stopStream.store(true);
    }
}

void enumerateDevices()
{
    deviceManager.getAvailableCameras();
    deviceManager.listAvailableCamerasByID();
    return;
}

void ptpSyncFreeRun(const std::list<std::shared_ptr<Camera>> &openCamerasList)
{
    networkManager.enablePtp(openCamerasList); // Should be before configureMultiCamerasNetwork
    networkManager.configureMultiCamerasNetwork(openCamerasList);
    networkManager.monitorPtpStatus(openCamerasList);
    networkManager.monitorPtpOffset(openCamerasList);
    networkManager.plotOffsets();
    return;
}

void startSyncFreeRunStream(const std::list<std::shared_ptr<Camera>> &openCamerasList, bool saveStream)
{
    streamManager.startSyncFreeRun(openCamerasList, stopStream, saveStream);
    return;
}

int main()
{
    std::signal(SIGINT, handleSignal);
    enumerateDevices();
    // deviceManager.openCameras({"Basler acA2500-14gm (21639790)"});     //home-camera
    std::list<std::string> cameraIDs = {"Basler acA2440-20gc (23630914)", "Basler acA2440-20gc (23630913)", "210200799"}; // ToDo, read from terminal
    deviceManager.openCameras(cameraIDs);
    std::list<std::shared_ptr<Camera>> openCamerasList = deviceManager.getopenCameras();
    deviceManager.listopenCameras();
    ptpSyncFreeRun(openCamerasList);
    // ToDo delete // streamManager.scheduleAcquisition(deviceManager.getopenCameras(), 1);
    bool saveStream = true;
    startSyncFreeRunStream(openCamerasList, saveStream);
    // Clean up
    rcg::System::clearSystems();
    deviceManager.~DeviceManager();
    networkManager.~NetworkManager();
    streamManager.~StreamManager();
    std::cout << "Application terminated gracefully.\n";
    return 0;
}