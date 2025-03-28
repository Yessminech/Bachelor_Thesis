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

class DeviceManager
{
public:
    DeviceManager();
    ~DeviceManager();

    // ToDo load Xml // void configureCamera(const std::string &cameraId, const std::string &params);

    bool getAvailableCameras();
    std::shared_ptr<rcg::Device> getAvailableCameraByID(const std::string &deviceId);
    std::shared_ptr<Camera> getOpenCameraByID(const std::string &deviceId);
    const std::list<std::shared_ptr<Camera>> &getopenCameras() const;
    const std::set<std::shared_ptr<rcg::Device>> &getAvailableCamerasList() const;
    void openCameras(std::list<std::string> deviceIds);
    // +closeCameras()  
    int64_t getScheduledTime(int64_t scheduledDelayS, std::string masterClockId);
    void scheduleActionCommands(const std::list<std::shared_ptr<Camera>> &openCamerasList, std::string masterClockId);

    bool listAvailableCamerasByID();
    bool listopenCameras();
    bool debug = true;

    uint32_t groupKey = 1;
    uint32_t groupMask = 0x00000001;

private:
    void enumerateDevicesFromSystems(const std::vector<std::shared_ptr<rcg::System>>& systems);
    bool openCamera(const std::string &deviceId);
    bool closeCamera(const std::string &deviceId);
    bool listCamera(std::shared_ptr<Camera> camera);

    std::list<std::shared_ptr<Camera>> openCamerasList;          // List of cameras
    std::set<std::shared_ptr<rcg::Device>> availableCamerasList; // List of cameras
    std::string defaultCti;
    
};

#endif // DEVICEMANAGER_HPP
