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
DeviceManager deviceManager ;
NetworkManager networkManager = NetworkManager(true, 800, 1000, 10);
StreamManager streamManager;

void handleSignal(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived Ctrl+C, stopping streams and cleanup...\n";
        stopStream.store(true);
    }
}

void enumerateDevices(){
    deviceManager.getAvailableCameras();
    deviceManager.listAvailableCamerasByID();
    return;
}

void ptpSyncFreeRun(const std::list<std::shared_ptr<Camera>>& openCamerasList){
    networkManager.enablePtp(openCamerasList); // Should be before configureNetworkFroSyncFreeRun
    networkManager.configureNetworkFroSyncFreeRun(openCamerasList);
   networkManager.monitorPtpStatus(openCamerasList, stopStream);
   networkManager.monitorPtpOffset(openCamerasList, stopStream);
    return;
}

void startSyncFreeRunStream(){
    const std::list<std::shared_ptr<Camera>>& openCamerasList = deviceManager.getopenCameras();
    bool saveStream = true;
    streamManager.startSyncFreeRun(openCamerasList, stopStream, saveStream);
    return;
}

 
int main()
{
    std::signal(SIGINT, handleSignal);
    enumerateDevices();
    //deviceManager.openCameras({"Basler acA2500-14gm (21639790)"});     //home-camera
    deviceManager.openCameras({"Basler acA2440-20gc (23630914)", "Basler acA2440-20gc (23630913)", "210200799"}); // ToDo, read from terminal 
    //deviceManager.openDevices({"Basler acA2440-20gc (23630914)"}); // ToDo, read from terminal 
    deviceManager.listopenCameras();
    ptpSyncFreeRun(deviceManager.getopenCameras());
    startSyncFreeRunStream();
    // Clean up
    rcg::System::clearSystems();
    deviceManager.~DeviceManager();
    networkManager.~NetworkManager();
    streamManager.~StreamManager();
    std::cout << "Application terminated gracefully.\n";
    return 0;
}