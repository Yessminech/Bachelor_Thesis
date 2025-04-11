#include "DeviceManager.hpp"
#include "GlobalSettings.hpp"
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

/**************************************************************************************/
/***********************         Constructor / Destructor     *************************/
/**************************************************************************************/

DeviceManager::DeviceManager()
{
    availableCamerasList = {};
    openCamerasList = {};
}

DeviceManager::~DeviceManager()
{
    availableCamerasList = {};
    for (auto &camera : openCamerasList)
    {
        closeCamera(camera->deviceInfos.id);
    }
}

/*******************************************************************************/
/***********************         Camera Operations     *************************/
/*******************************************************************************/

void DeviceManager::getAvailableCameras()
{
    // Clear existing available cameras
    availableCamerasList.clear();

    // First try with system-provided paths
    std::vector<std::shared_ptr<rcg::System>> systems;
    try
    {
        systems = rcg::System::getSystems();
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to get systems: " << ex.what() << RESET << std::endl;
        return;
    }

    if (!systems.empty())
    {
        enumerateDevicesFromSystems(systems);
    }
    else
    {
        std::cerr << RED << "No systems found from default paths." << RESET << std::endl;
    }

    // Then try with the custom path
    try
    {
        if (!defaultCti.empty())
        {
            rcg::System::setSystemsPath(defaultCti.c_str(), nullptr);
            std::vector<std::shared_ptr<rcg::System>> defaultSystems = rcg::System::getSystems();
            if (!defaultSystems.empty())
            {
                enumerateDevicesFromSystems(defaultSystems);
            }
            else
            {
                std::cerr << RED << "No systems found from custom path." << RESET << std::endl;
            }
        }
        else
        {
            std::cerr << RED << "Custom CTI path is empty." << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error enumerating with custom path: " << ex.what() << RESET << std::endl;
    }
    return;
}

void DeviceManager::createCameras(std::list<std::string> deviceIds)
{
    for (const auto &deviceId : deviceIds)
    {
        createCamera(deviceId);
    }
    return;
}

uint32_t generateRandomKey()
{
    srand( (unsigned) time( NULL ) );
    return rand();
}

void DeviceManager::scheduleActionCommands(const std::list<std::shared_ptr<Camera>> &openCamerasList, std::string masterClockId)
{
    uint32_t actionGroupKey = 1;
    uint32_t actionGroupMask = 0x00000001;
    int64_t scheduledDelayS = 1; // 1 second delay

    for (const auto &camera : openCamerasList)
    {
        uint32_t actionDeviceKey = generateRandomKey();
        camera->setScheduledActionCommand(actionDeviceKey, groupKey, groupMask);
        int64_t scheduledTime = getScheduledTime(1, masterClockId); // 1 second delay
        camera->issueScheduledActionCommand(actionDeviceKey, actionGroupKey, actionGroupMask, scheduledDelayS);
    }
}

/*******************************************************************************/
/***********************         Cameras Enumeration     *************************/
/*******************************************************************************/

const std::set<std::shared_ptr<rcg::Device>> &DeviceManager::getAvailableCamerasList() const{
    return availableCamerasList;
}

std::shared_ptr<rcg::Device> DeviceManager::getAvailableCameraByID(const std::string &deviceId)
{
    for (const auto &device : availableCamerasList)
    {
        if (device->getID() == deviceId)
        {
            return device;
        }
    }
    return nullptr;
}

std::list<std::string> DeviceManager::listAvailableCamerasByID()
{
    try
    {
        std::list<std::string> availableCamerasIds;
        for (const std::shared_ptr<rcg::Device> &device : availableCamerasList)
        {
            availableCamerasIds.push_back(device->getID());
        }
        std::cout << "  Available Devices IDs:  ";
        std::cout << std::endl;
        for (const auto &id : availableCamerasIds)
        {
            std::cout << "        Device:             " << id << std::endl;
            std::cout << std::endl;
        }
        std::cout << std::endl;
        return availableCamerasIds;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to list devices: " << ex.what() << RESET << std::endl;
        return {};
    }
}

const std::list<std::shared_ptr<Camera>> &DeviceManager::getOpenCameras() const
{
    return openCamerasList;
}

std::shared_ptr<Camera> DeviceManager::getOpenCameraByID(const std::string &deviceId)
{
    for (const auto &camera : openCamerasList)
    {
        DeviceInfos deviceInfos = camera->deviceInfos;
        if (deviceInfos.id == deviceId)
        {
            return camera;
        }
    }
    return nullptr;
}

bool DeviceManager::listopenCameras()
{
    try
    {
        for (const auto &camera : openCamerasList)
        {
            listCamera(camera);
        }
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to list devices: " << ex.what() << RESET << std::endl;
        return false;
    }
}

/*********************************************************************/
/***********************         Setters     *************************/
/*********************************************************************/

void DeviceManager::setPixelFormatAll(std::string pixelFormat)
{
    for (const auto &camera : openCamerasList)
    {
        camera->setPixelFormat(pixelFormat);
    }
}

void DeviceManager::setExposureTimeAll(double exposureTime)
{
    for (const auto &camera : openCamerasList)
    {
        camera->setExposureTime(exposureTime);
    }
}

void DeviceManager::setGainAll(float gain)
{
    for (const auto &camera : openCamerasList)
    {
        camera->setGain(gain);
    }
}

void DeviceManager::setHeightAll(int height)
{
    for (const auto &camera : openCamerasList)
    {
        camera->setHeight(height);
    }
}

void DeviceManager::setWidthAll(int width)
{
    for (const auto &camera : openCamerasList)
    {
        camera->setWidth(width);
    }
}

/***********************         Private Methods     *************************/

/*******************************************************************************/
/***********************         Camera Operations     *************************/
/*******************************************************************************/

bool DeviceManager::createCamera(const std::string &deviceId)
{
    try
    {
        std::shared_ptr<rcg::Device> device = getAvailableCameraByID(deviceId);
        if (!device)
        {
            std::cerr << RED << "Failed to create camera: " << deviceId << RESET << std::endl;
            return false;
        }

        std::shared_ptr<Camera> newCamera = std::make_shared<Camera>(device);
        openCamerasList.push_back(newCamera);
        if (debug)
        {
            std::cout << GREEN << "[DEBUG] Camera " << deviceId << ": Added to open Cameras successfully" << RESET << std::endl;
        }
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to add camera: " << ex.what() << RESET << std::endl;
        return false;
    }
}

bool DeviceManager::closeCamera(const std::string &deviceId)
{
    try
    {
        std::shared_ptr<Camera> camera = getOpenCameraByID(deviceId);
        camera->~Camera();
        openCamerasList.remove(camera);
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to remove camera: " << ex.what() << RESET << std::endl;
        return false;
    }
}

/******************************************************************************/
/***********************         Internal Helpers     *************************/
/******************************************************************************/

void DeviceManager::enumerateDevicesFromSystems(const std::vector<std::shared_ptr<rcg::System>> &systems)
{
    for (auto &system : systems)
    {
        try
        {
            system->open();

            std::vector<std::shared_ptr<rcg::Interface>> interfaces = system->getInterfaces();
            if (interfaces.empty())
            {
                system->close();
                continue;
            }

            for (auto &interf : interfaces)
            {
                try
                {
                    interf->open();

                    std::vector<std::shared_ptr<rcg::Device>> devices = interf->getDevices();
                    for (auto &device : devices)
                    {
                        // Only process valid devices that match the vendor
                        if (device && device->getVendor() == system->getVendor())
                        {
                            // Check if we already have this device ID
                            if (!getAvailableCameraByID(device->getID()))
                            {
                                availableCamerasList.insert(device);
                            }
                        }
                    }

                    interf->close();
                }
                catch (const std::exception &ex)
                {
                    std::cerr << RED << "Failed to process interface: " << ex.what() << RESET << std::endl;
                }
            }

            system->close();
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Failed to open system: " << ex.what() << RESET << std::endl;
        }
    }
}

int64_t DeviceManager::getScheduledTime(int64_t scheduledDelayS, std::string masterClockId)
{
    auto masterCam = getOpenCameraByID(masterClockId);
    masterCam->getTimestamps();
    int64_t currentTimestamp = masterCam->ptpConfig.timestamp_ns;
    return currentTimestamp + scheduledDelayS * 1e9; // delay in nanoseconds
}

bool DeviceManager::listCamera(std::shared_ptr<Camera> camera)
{
    try
    {
        DeviceInfos config = camera->deviceInfos;
 <       std::cout << "       Device:             " << config.id << std::endl;
        std::cout << "        Vendor:            " << config.vendor << std::endl;
        std::cout << "        Model:             " << config.model << std::endl;
        std::cout << "        TL type:           " << config.tlType << std::endl;
        std::cout << "        Display name:      " << config.displayName << std::endl;
        std::cout << "        Access status:     " << config.accessStatus << std::endl;
        std::cout << "        Serial number:     " << config.serialNumber << std::endl;
        std::cout << "        Current IP:        " << config.currentIP << std::endl;
        std::cout << "        MAC:               " << config.MAC << std::endl;
        std::cout << "        Deprecated FW:     " << (config.deprecatedFW ? "Yes" : "No") << std::endl;
        std::cout << std::endl;>
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to list devices: " << ex.what() << RESET << std::endl;
        return false;
    }
}

