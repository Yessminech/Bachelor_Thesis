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

class DeviceManager
{
public:
    // === Constructor / Destructor ===
    DeviceManager();
    ~DeviceManager();

    // === Camera Operations ===
    void getAvailableCameras();
    void createCameras(std::list<std::string> deviceIds);
    void scheduleActionCommands(const std::list<std::shared_ptr<Camera>> &openCamerasList, std::string masterClockId);

    // === Cameras Enumeration ===
    const std::set<std::shared_ptr<rcg::Device>> &getAvailableCamerasList() const;
    std::shared_ptr<rcg::Device> getAvailableCameraByID(const std::string &deviceId);
    std::list<std::string> listAvailableCamerasByID();
    const std::list<std::shared_ptr<Camera>> &getOpenCameras() const;
    std::shared_ptr<Camera> getOpenCameraByID(const std::string &deviceId);
    bool listopenCameras();

    // === Setters ===
    void setPixelFormatAll(std::string pixelFormat);
    void setExposureTimeAll(double exposureTime);
    void setGainAll(float gain);
    void setHeightAll(int height);
    void setWidthAll(int width);

    // === Configuration ===
    uint32_t groupKey = 1;
    uint32_t groupMask = 0x00000001;

private:
    // === Camera Operations ===
    bool createCamera(const std::string &deviceId);
    bool closeCamera(const std::string &deviceId);

    // === Internal Helpers ===
    void enumerateDevicesFromSystems(const std::vector<std::shared_ptr<rcg::System>> &systems);
    int64_t getScheduledTime(int64_t scheduledDelayS, std::string masterClockId);
    bool listCamera(std::shared_ptr<Camera> camera);

    // === Private Members ===
    std::list<std::shared_ptr<Camera>> openCamerasList;
    std::set<std::shared_ptr<rcg::Device>> availableCamerasList;
};

#endif // DEVICEMANAGER_HPP
