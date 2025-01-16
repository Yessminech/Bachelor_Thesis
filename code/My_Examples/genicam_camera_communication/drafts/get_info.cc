#include <iostream>
#include <GenTL/GenTL_v1_6.h>

#include <vector>
#include <cstring>

void listDevices() {
TL_HANDLE hSystem;
IF_HANDLE hInterface;
DEV_HANDLE hDevice;
// Load the first available GenTL Producer
GenTL::GC_ERROR err = GenTL::TLOpenSystem(&hSystem);
if (err != GenTL::GC_ERR_SUCCESS) {
    std::cerr << "Failed to open GenTL system" << std::endl;
    return;
}

// Get the number of available interfaces
uint32_t numInterfaces = 0;
err = GenTL::TLGetNumInterfaces(hSystem, &numInterfaces);
if (err != GenTL::GC_ERR_SUCCESS || numInterfaces == 0) {
    std::cerr << "No interfaces found" << std::endl;
    GenTL::TLCloseSystem(hSystem);
    return;
}

// Iterate through each interface
for (uint32_t i = 0; i < numInterfaces; i++) {
    char interfaceID[256];
    size_t size = sizeof(interfaceID);

    // Get the interface ID
    err = GenTL::TLGetInterfaceID(hSystem, i, interfaceID, &size);
    if (err != GenTL::GC_ERR_SUCCESS) {
        continue;
    }

    // Open the interface
    err = GenTL::TLGetInterfaceHandle(hSystem, interfaceID, &hInterface);
    if (err != GenTL::GC_ERR_SUCCESS) {
        continue;
    }
    GenTL::IFOpen(hInterface);

    // Get the number of devices on the interface
    uint32_t numDevices = 0;
    err = GenTL::IFGetNumDevices(hInterface, &numDevices);
    if (err != GenTL::GC_ERR_SUCCESS || numDevices == 0) {
        GenTL::IFClose(hInterface);
        continue;
    }

    std::cout << "Interface: " << interfaceID << std::endl;

    // List all devices
    for (uint32_t j = 0; j < numDevices; j++) {
        char deviceID[256];
        size = sizeof(deviceID);

        // Get the device ID
        err = GenTL::IFGetDeviceID(hInterface, j, deviceID, &size);
        if (err == GenTL::GC_ERR_SUCCESS) {
            std::cout << "  Device: " << deviceID << std::endl;
        }
    }

    // Close the interface
    GenTL::IFClose(hInterface);
}

// Close the system
GenTL::TLCloseSystem(hSystem);
}

int main() {
listDevices();
return 0;
}