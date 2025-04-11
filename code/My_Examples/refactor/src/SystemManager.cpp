#include "SystemManager.hpp"
#include "DeviceManager.hpp"
#include "NetworkManager.hpp"
#include "StreamManager.hpp"
#include "Camera.hpp"
#include "GlobalSettings.hpp"

#include <rc_genicam_api/system.h>

#include <csignal>
#include <iostream> // For std::cout
#include <list>
#include <memory>
#include <string>
#include <chrono>
#include <stdexcept> // For std::exception
#include <atomic>    // For std::atomic

// Global variable for signal handling
std::atomic<bool> stopStream{false};

void handleSignal(int signal)
{
    if (signal == SIGINT)
    {
        std::cout << "\nReceived Ctrl+C, stopping streams and cleanup...\n";
        stopStream.store(true);
    }
}

/**************************************************************************************/
/***********************         Constructor / Destructor     *************************/
/**************************************************************************************/

// Constructor
SystemManager::SystemManager()
{
}

// Destructor
SystemManager::~SystemManager()
{
}

/*****************************************************************************/
/***********************         System Lifecyle     *************************/
/*****************************************************************************/

void SystemManager::initializeSystem()
{
    try
    {
        std::signal(SIGINT, handleSignal);
        enumerateCameras();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during system initialization: " << e.what() << std::endl;
        throw;
    }
}

void SystemManager::shutdownSystem()
{
    try
    {
        rcg::System::clearSystems();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during system shutdown: " << e.what() << std::endl;
    }
}

/********************************************************************************/
/***********************         Device Enumeration     *************************/
/********************************************************************************/

void SystemManager::enumerateCameras()
{
    try
    {
        std::list<std::shared_ptr<Camera>> availableCameras = deviceManager.getAvailableCameras();
        std::list<std::string> availableCamerasIds = deviceManager.listAvailableCamerasByID();

        if (availableCameras.empty())
        {
            throw std::runtime_error("No cameras found. Please check the connections.");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during device enumeration: " << e.what() << std::endl;
        throw;
    }
}

void SystemManager::enumerateOpenCameras()
{
    try
    {
        deviceManager.listopenCameras();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error listing available cameras: " << e.what() << std::endl;
        throw;
    }
}

/*****************************************************************************/
/***********************         Feature Setting     *************************/
/*****************************************************************************/
// ToDo
// void SystemManager::setFeature(std::string cameraID, std::string featureName, std::string value)
// {
//     try
//     {
//         if (featureName == "")
//         {
//             deviceManager.setFeature(cameraID, featureName, value);
//         }
//         else
//         {
//             throw std::invalid_argument("Invalid feature name: " + featureName);
//         }

//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "Error setting feature: " << e.what() << std::endl;
//         throw;
//     }
// }

/***********************************************************************/
/***********************         Streaming     *************************/
/***********************************************************************/

void SystemManager::syncFreeRunStream(std::list<std::string> camerasIDs, bool saveStream, std::chrono::milliseconds acquisitionDelay)
{
    try
    {
        deviceManager.createCameras(camerasIDs);
        std::list<std::shared_ptr<Camera>> openCamerasList = deviceManager.getOpenCameras();

        if (openCamerasList.empty())
        {
            throw std::runtime_error("No cameras were successfully opened. Please check the camera IDs and connections.");
        }

        if (openCamerasList.size() > 1)
        {
            networkManager.configurePtpSyncFreeRun(openCamerasList);
        }

        streamManager.startPtpSyncFreeRun(openCamerasList, stopStream, saveStream, acquisitionDelay);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during syncFreeRunStream: " << e.what() << std::endl;
        throw;
    }
}

/****************************************************************************/
/***********************         PTP Management     *************************/
/****************************************************************************/

bool SystemManager::ptpEnable(std::list<std::string> camerasIDs)
{
    try
    {
        deviceManager.createCameras(camerasIDs);
        std::list<std::shared_ptr<Camera>> openCamerasList = deviceManager.getOpenCameras();
        networkManager.enablePtp(openCamerasList);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during PTP enable: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool SystemManager::ptpDisable(std::list<std::string> camerasIDs)
{
    try
    {
        deviceManager.createCameras(camerasIDs);
        std::list<std::shared_ptr<Camera>> openCamerasList = deviceManager.getOpenCameras();
        networkManager.disablePtp(openCamerasList);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during PTP disable: " << e.what() << std::endl;
        return false;
    }
    return true;
}