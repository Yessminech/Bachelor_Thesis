#include "DeviceManager.hpp"
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

void enumerateDevices(){
    // device enumeration 
    DeviceManager deviceManager;
    deviceManager.getAvailableCameras();
    deviceManager.listAvailableCamerasByID();
    return;
}

int main()
{
    enumerateDevices();
    return 0;
}