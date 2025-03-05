#ifndef DEVICEMANAGER_HPP
#define DEVICEMANAGER_HPP

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

#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    bool listDevicesByIdOrIP(const std::string &id, const std::string &ip, std::shared_ptr<Camera> camera);
    bool listDevices(std::shared_ptr<Camera> camera);
    bool listDevicesIDs(std::shared_ptr<Camera> camera);

    // Adds a new camera with configuration
    void addCamera(const std::string &cameraId, const std::string &config);

    // Removes a camera from the system
    void removeCamera(const std::string &cameraId);

    // Returns a list of available cameras
    std::set<std::string> getCameraList() const;

    // Retrieves the health/status of a specific camera
    std::string getCameraStatus(const std::string &cameraId) const;

    // Configures a camera’s settings
    void configureCamera(const std::string &cameraId, const std::string &params);

    double numCams; // Number of cameras
    bool debug;     // Debug flag
    std::vector<std::shared_ptr<rcg::Device>> globalDevices;
};

#endif // DEVICEMANAGER_HPP