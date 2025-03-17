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

DeviceManager::DeviceManager()
{
    availableCamerasList = {};
    openCamerasList = {};
    defaultCti = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib"; // ToDo add other path mvImpact?
}

DeviceManager::~DeviceManager()
{
    availableCamerasList = {};
    for (auto &camera : openCamerasList)
    {
        closeCamera(camera->deviceConfig.id);
        if (debug)
            std::cout << GREEN << "[DEBUG] Camera " << camera->deviceConfig.id << ": Closed successfully" << RESET << std::endl;
    }
}

bool DeviceManager::getAvailableCameras()
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
        return false;
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

    return !availableCamerasList.empty();
}

void DeviceManager::openCameras(std::list<std::string> deviceIds){
    // device enumeration 
    for (const auto &deviceId : deviceIds){
        openCamera(deviceId);
    }
    return;
}

// Helper method to enumerate devices from a list of systems
void DeviceManager::enumerateDevicesFromSystems(const std::vector<std::shared_ptr<rcg::System>>& systems)
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

bool DeviceManager::listAvailableCamerasByID()
{
    try
    {
        std::list<std::string> availableCamerasIds;
        for (const std::shared_ptr<rcg::Device> &device : availableCamerasList)
        {
            availableCamerasIds.push_back(device->getID());
        }
        std::cout << "  Available Devices IDs:  ";
        for (const auto &id : availableCamerasIds)
        {
            std::cout << "\"" << id << "\" ";
        }
        std::cout << std::endl;
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to list devices: " << ex.what() << RESET << std::endl;
        return false;
    }
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

const std::list<std::shared_ptr<Camera>> &DeviceManager::getopenCameras() const
{
    return openCamerasList;
}

std::shared_ptr<Camera> DeviceManager::getOpenCameraByID(const std::string &deviceId)
{
    for (const auto &camera : openCamerasList)
    {
        DeviceConfig deviceConfig = camera->deviceConfig;
        if (deviceConfig.id == deviceId)
        {
            return camera;
        }
    }
    return nullptr;
}

bool DeviceManager::openCamera(const std::string &deviceId)
{
    try
    {
        std::shared_ptr<rcg::Device> device = getAvailableCameraByID(deviceId);
        if (!device)
        {
            std::cerr << RED << "Failed to open camera: " << deviceId << RESET << std::endl;
            return false;
        }
        std::shared_ptr<Camera> newCamera = std::make_shared<Camera>(device);
        openCamerasList.push_back(newCamera);
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

bool DeviceManager::listCamera(std::shared_ptr<Camera> camera)
{
    try
    {
        DeviceConfig config = camera->deviceConfig;
        std::cout << "        Device             " << config.id << std::endl;
        std::cout << "        Vendor:            " << config.vendor << std::endl;
        std::cout << "        Model:             " << config.model << std::endl;
        std::cout << "        TL type:           " << config.tlType << std::endl;
        std::cout << "        Display name:      " << config.displayName << std::endl;
        std::cout << "        Access status:     " << config.accessStatus << std::endl;
        std::cout << "        Serial number:     " << config.serialNumber << std::endl;
        std::cout << "        Version:           " << config.version << std::endl;
        std::cout << "        TS Frequency:      " << config.timestampFrequency << std::endl;
        std::cout << "        Current IP:        " << config.currentIP << std::endl;
        std::cout << "        MAC:               " << config.MAC << std::endl;
        std::cout << "        Deprecated FW:     " << (config.deprecatedFW ? "Yes" : "No") << std::endl;
        std::cout << std::endl;
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to list devices: " << ex.what() << RESET << std::endl;
        return false;
    }
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