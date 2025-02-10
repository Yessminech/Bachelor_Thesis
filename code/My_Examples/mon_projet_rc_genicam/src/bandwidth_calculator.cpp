#include <iostream>
#include <vector>
#include <GenApi/GenApi.h>
#include <GenTL.h>

using namespace std;
using namespace GenApi;

// Function to compute Packet Delay (GevSCPD)
double CalculatePacketDelay(double packetSize, double deviceLinkSpeed, double bufferPercent) {
    double buffer = (bufferPercent / 100.0) * (packetSize * (1e9 / deviceLinkSpeed));
    return (packetSize * (1e9 / deviceLinkSpeed)) + buffer;
}

// Function to configure cameras using GenTL
void ConfigureCameras(int numCameras, double bufferPercent) {
    GenTL::TL_HANDLE hTL;
    GenTL::IF_HANDLE hInterface;
    GenTL::DEV_HANDLE hDevice;
    GenTL::DS_HANDLE hDataStream;

    // Load Transport Layer (TL)
    GenTL::GC_ERROR status = GenTL::GCInitLib();
    if (status != GenTL::GC_ERR_SUCCESS) {
        cerr << "Error: Failed to initialize GenTL!" << endl;
        return;
    }

    // Open Transport Layer
    status = GenTL::TLOpen(&hTL);
    if (status != GenTL::GC_ERR_SUCCESS) {
        cerr << "Error: Unable to open Transport Layer!" << endl;
        return;
    }

    // Open first available interface
    status = GenTL::IFOpen(hTL, 0, &hInterface);
    if (status != GenTL::GC_ERR_SUCCESS) {
        cerr << "Error: Unable to open Interface!" << endl;
        return;
    }

    for (int i = 0; i < numCameras; ++i) {
        // Open camera device
        status = GenTL::DevOpen(hInterface, i, &hDevice);
        if (status != GenTL::GC_ERR_SUCCESS) {
            cerr << "Error: Failed to open camera " << i + 1 << "!" << endl;
            continue;
        }

        // Get Packet Size
        int64_t packetSize;
        status = GenTL::DevGetParamInt(hDevice, "GevSCPSPacketSize", &packetSize);
        if (status != GenTL::GC_ERR_SUCCESS) {
            cerr << "Error: Unable to read Packet Size for camera " << i + 1 << endl;
            continue;
        }

        // Get Device Link Speed
        int64_t deviceLinkSpeed;
        status = GenTL::DevGetParamInt(hDevice, "DeviceLinkSpeed", &deviceLinkSpeed);
        if (status != GenTL::GC_ERR_SUCCESS) {
            cerr << "Error: Unable to read Device Link Speed for camera " << i + 1 << endl;
            continue;
        }

        // Compute Packet Delay
        double packetDelay = CalculatePacketDelay(packetSize, deviceLinkSpeed, bufferPercent);

        // Set GevSCPD
        int64_t gevSCPDValue = static_cast<int64_t>(packetDelay * (i));
        status = GenTL::DevSetParamInt(hDevice, "GevSCPD", gevSCPDValue);
        if (status == GenTL::GC_ERR_SUCCESS) {
            cout << "Camera " << i + 1 << " GevSCPD set to: " << gevSCPDValue << " ns" << endl;
        } else {
            cerr << "Error: Unable to set GevSCPD for camera " << i + 1 << endl;
        }

        // Set GevSCFTD
        int64_t gevSCFTDValue = static_cast<int64_t>(packetDelay * (i - 1));
        status = GenTL::DevSetParamInt(hDevice, "GevSCFTD", gevSCFTDValue);
        if (status == GenTL::GC_ERR_SUCCESS) {
            cout << "Camera " << i + 1 << " GevSCFTD set to: " << gevSCFTDValue << " ns" << endl;
        } else {
            cerr << "Error: Unable to set GevSCFTD for camera " << i + 1 << endl;
        }

        // Close camera
        GenTL::DevClose(hDevice);
    }

    // Close transport layer
    GenTL::IFClose(hInterface);
    GenTL::TLClose(hTL);
    GenTL::GCCloseLib();
}

int main() {
    int numCameras;
    double bufferPercent;

    cout << "Enter the number of cameras: ";
    cin >> numCameras;

    cout << "Enter buffer percentage (10-30% recommended): ";
    cin >> bufferPercent;

    if (numCameras < 1 || bufferPercent < 10 || bufferPercent > 30) {
        cerr << "Invalid input. Please enter a valid number of cameras and buffer percentage (10-30%)." << endl;
        return -1;
    }

    ConfigureCameras(numCameras, bufferPercent);

    return 0;
}