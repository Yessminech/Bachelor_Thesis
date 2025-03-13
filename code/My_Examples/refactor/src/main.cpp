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
NetworkManager networkManager;
StreamManager streamManager;

void handleSignal(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived Ctrl+C, stopping streams and cleanup...\n";
        stopStream.store(true);
    }
}

void enumerateDevices(){
    // device enumeration 
    deviceManager.getAvailableCameras();
    deviceManager.listAvailableCamerasByID();
    return;
}

void startSyncFreeRunStream(){
    const std::list<std::shared_ptr<Camera>>& openCamerasList = deviceManager.getOpenCameras();
    bool saveStream = true;
    networkManager.configureNetworkFroSyncFreeRun(openCamerasList);
    streamManager.startSyncFreeRun(openCamerasList, stopStream, saveStream);
    return;
}

 
int main()
{
    std::signal(SIGINT, handleSignal);
    enumerateDevices();
    //home-cam
    deviceManager.openCameras({"Basler acA2500-14gm (21639790)"}); 
    //deviceManager.openCameras({"Basler acA2440-20gc (23630914)", "Basler acA2440-20gc (23630913)", "210200799"}); // ToDo, read from terminal 
    //deviceManager.openDevices({"Basler acA2440-20gc (23630914)"}); // ToDo, read from terminal 
    deviceManager.listOpenCameras();
    startSyncFreeRunStream();
    // Clean up
    rcg::System::clearSystems();
    deviceManager.~DeviceManager();
    networkManager.~NetworkManager();
    streamManager.~StreamManager();
    std::cout << "Application terminated gracefully.\n";
    return 0;
}