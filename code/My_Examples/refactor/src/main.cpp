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

volatile sig_atomic_t stop = false;
DeviceManager deviceManager;
NetworkManager networkManager;
StreamManager streamManager;

void handleSignal(int signal) {
    if (signal == SIGINT) {
        stop = true;
    }
}

void enumerateDevices(){
    // device enumeration 
    deviceManager.getAvailableCameras();
    deviceManager.listAvailableCamerasByID();
    return;
}

void openDevices(std::list<std::string> deviceIds){
    // device enumeration 
    for (const auto &deviceId : deviceIds){
        deviceManager.openCamera(deviceId);
    }
    return;
}

void startSyncFreeRunStream(){
    const std::list<std::shared_ptr<Camera>>& openCameras = deviceManager.getOpenCameras();
    networkManager.configureNetworkFroSyncFreeRun(openCameras);
    streamManager.startSyncFreeRun(openCameras, false);
    return;
}

 
int main()
{
    std::signal(SIGINT, handleSignal);
    enumerateDevices();
    //openDevices({"Basler acA2440-20gc (23630914)", "Basler acA2440-20gc (23630913)", "210200799"}); // ToDo, read from terminal 
    openDevices({"Basler acA2440-20gc (23630914)"}); // ToDo, read from terminal 
    deviceManager.listOpenCameras();
    startSyncFreeRunStream();
    while (!stop){
        // Add your logic here to update the stop condition
    }
    rcg::System::clearSystems(); // not forget
    deviceManager.~DeviceManager();
    networkManager.~NetworkManager();
    streamManager.~StreamManager();
    return 0;
}