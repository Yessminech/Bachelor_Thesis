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
    availableCameras = {};
    openCameras = {};
    defaultCtiPath = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib"; // ToDo add other path mvImpact?
}

DeviceManager::~DeviceManager()
{
    availableCameras = {};
    for (auto &camera : openCameras)
    {
        closeCamera(camera->deviceConfig.id);
    }
}

bool DeviceManager::getAvailableCameras()
{
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
    if (systems.empty())
    {
        std::cerr << RED << "Error: No systems found." << RESET << std::endl;
        return false;
    }
    for (auto &system : systems)
    {
        try
        {
            system->open();
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "[Camera::debug] Failed to open system: " << ex.what() << RESET << std::endl;
            continue;
        }
        std::vector<std::shared_ptr<rcg::Interface>> interfaces = system->getInterfaces();
        if (interfaces.empty())
            continue;
        for (auto &interf : interfaces)
        {
            try
            {
                interf->open();
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[Camera::debug] Failed to open interface: " << ex.what() << RESET << std::endl;
                continue;
            }
            std::vector<std::shared_ptr<rcg::Device>> devices = interf->getDevices();
            if (devices.empty())
                continue;
            for (auto &device : devices)
            {
                if (!device || device->getVendor() != system->getVendor())
                {
                    interf->close();
                    continue;
                }
                if (getAvailableCameraByID(device->getID()))
                {
                    interf->close();
                    continue;
                }
                availableCameras.insert(device);
            }
            interf->close();
        }
        system->close();
    }

    try
    {
        if (defaultCtiPath.empty())
        {
            std::cerr << RED << "Environment variable GENICAM_GENTL64_PATH is not set." << RESET << std::endl;
            return false;
        }
        rcg::System::setSystemsPath(defaultCtiPath.c_str(), nullptr);
        std::vector<std::shared_ptr<rcg::System>> defaultSystems = rcg::System::getSystems();
        if (defaultSystems.empty())
        {
            std::cerr << RED << "Error: No systems found." << RESET << std::endl;
            return false;
        }
        for (auto &system : defaultSystems)
        {
            try
            {
                system->open();
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[Camera::debug] Failed to open system: " << ex.what() << RESET << std::endl;
                continue;
            }
            std::vector<std::shared_ptr<rcg::Interface>> interfaces = system->getInterfaces();
            if (interfaces.empty())
                continue;
            for (auto &interf : interfaces)
            {
                try
                {
                    interf->open();
                }
                catch (const std::exception &ex)
                {
                    std::cerr << RED << "[Camera::debug] Failed to open interface: " << ex.what() << RESET << std::endl;
                    continue;
                }
                
                std::vector<std::shared_ptr<rcg::Device>> devices = interf->getDevices();
                if (devices.empty())
                    continue;
                for (auto &device : devices)
                {
                 if (!device)
                {
                    interf->close();
                    continue;
                }
                if (getAvailableCameraByID(device->getID()))
                {
                    interf->close();
                    continue;
                }
                availableCameras.insert(device);
            }
                interf->close();
            }
            system->close();
        }
    }
    catch (const std::exception &ex)
    {
        std::cout << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
        return false;
    }
    return true;
}

bool DeviceManager::listAvailableCamerasByID()
{
    try
    {
        std::list<std::string> availableCamerasIds;

        for (const auto &device : availableCameras)
        {
            availableCamerasIds.push_back(device->getSerialNumber());
        }
        std::cout << "  Available Devices IDs:  ";
        for (const auto &id : availableCamerasIds)
        {
            std::cout << id << " ";
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
    for (const auto &device : availableCameras)
    {
        if (device->getID() == deviceId)
        {
            return device;
        }
    }
    return nullptr;
}

std::shared_ptr<Camera> DeviceManager::getOpenCameraByID(const std::string &deviceId)
{
    for (const auto &camera : openCameras)
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
        openCameras.push_back(newCamera);
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
        openCameras.remove(camera);
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
        std::cout << std::endl;
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to list devices: " << ex.what() << RESET << std::endl;
        return false;
    }
}

bool DeviceManager::listOpenCameras()
{
    try
    {
        for (const auto &camera : openCameras)
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