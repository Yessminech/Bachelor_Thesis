#ifndef SYSTEM_MANAGER_HPP
#define SYSTEM_MANAGER_HPP
#include "SystemManager.hpp"
#include "DeviceManager.hpp"
#include "NetworkManager.hpp"
#include "StreamManager.hpp"
#include "Camera.hpp"
#include <rc_genicam_api/device.h>

#include <vector>
#include <memory>
#include <list>
#include <string>
#include <atomic>
#include <chrono>

class SystemManager {
public:

    // === Constructor / Destructor ===
    SystemManager();
    ~SystemManager();

    // === System Lifecycle ===
    void initializeSystem(); // Initializes all managers and system components
    void shutdownSystem(); // Gracefully stops all operations
    
    // === Device Enumeration ===
    void enumerateCameras(); // Enumerates available cameras 
    void enumerateOpenCameras(); // Enumerates open cameras 

    // === Feature setting ===
    void setFeature(std::string cameraID, std::string featureName, std::string value); // Sets a feature for a specific camera
    // void setFeature(std::string cameraID, std::string featureName, int value); // Sets a feature for a specific camera
    // void setFeature(std::string cameraID, std::string featureName, double value); // Sets a feature for a specific camera
    // void setFeature(std::string cameraID, std::string featureName, bool value); // Sets a feature for a specific camera
    // void setFeature(std::string cameraID, std::string featureName, const char* value); // Sets a feature for a specific camera

    // === Streaming ===
    void syncFreeRunStream(std::list<std::string>  camerasIDs, bool saveStream, std::chrono::milliseconds acquisitionDelay);
    
    // === PTP Management ===   
    bool ptpEnable(std::list<std::string> camerasIDs);
    bool ptpDisable(std::list<std::string> camerasIDs); 

    // === Public Members ===
    std::atomic<bool> stopStream{false}; // Correct initialization
    DeviceManager deviceManager;
    NetworkManager networkManager;
    StreamManager streamManager;
};

#endif // SYSTEM_MANAGER_HPP