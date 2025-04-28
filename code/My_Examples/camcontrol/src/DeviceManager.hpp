/**
 * @brief Library component of the TU Berlin industrial automation framework
 *
 * Copyright (c) 2025
 * TU Berlin, Institut für Werkzeugmaschinen und Fabrikbetrieb
 * Fachgebiet Industrielle Automatisierungstechnik
 * Author: Yessmine Chabchoub
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written permission.
 *
 * @note
 * DISCLAIMER
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DAMAGES ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE.
 */

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

/**
 * @brief DeviceManager class handles camera discovery, initialization, and configuration.
 */
class DeviceManager
{
public:
    /**
     * @brief Constructs a DeviceManager instance and initialize cameras lists.
     */
    DeviceManager();
    ~DeviceManager();

    /**
     * @brief Discovers available cameras on the network.
     */
    void getAvailableCameras();

    /**
     * @brief Creates Camera objects for the specified device IDs.
     */
    void createCameras(std::list<std::string> deviceIds);

    /**
     * @brief Returns the list of available cameras.
     */
    const std::set<std::shared_ptr<rcg::Device>> &getAvailableCamerasList() const;

    /**
     * @brief Returns a device by its ID.
     */
    std::shared_ptr<rcg::Device> getAvailableCameraByID(const std::string &deviceId);

    /**
     * @brief Returns a device by its serial number.
     */
    std::shared_ptr<rcg::Device> getAvailableCameraBySN(const std::string &deviceSN);

    /**
     * @brief Lists all available camera IDs.
     */
    std::list<std::string> listAvailableCamerasByID();

    /**
     * @brief Returns the list of currently open cameras.
     */
    const std::list<std::shared_ptr<Camera>> &getOpenCameras() const;

    /**
     * @brief Returns an open camera by its ID.
     */
    std::shared_ptr<Camera> getOpenCameraByID(const std::string &deviceId);

    /**
     * @brief Lists the currently open cameras.
     */
    bool listopenCameras();

    /**
     * @brief Sets the pixel format for all open cameras.
     */
    void setPixelFormatAll(std::string pixelFormat);

    /**
     * @brief Sets the exposure time for all open cameras.
     */
    void setExposureTimeAll(double exposureTime);

    /**
     * @brief Sets the gain value for all open cameras.
     */
    void setGainAll(float gain);

    /**
     * @brief Sets the height for all open cameras.
     */
    void setHeightAll(int height);

    /**
     * @brief Sets the width for all open cameras.
     */
    void setWidthAll(int width);

    /**
     * @brief Group key for multi-camera synchronization.
     */
    uint32_t groupKey = 1;

    /**
     * @brief Group mask for multi-camera synchronization.
     */
    uint32_t groupMask = 0x00000001;

private:
    /**
     * @brief Creates a camera by device ID.
     */
    bool createCamera(const std::string &deviceId);

    /**
     * @brief Closes a camera by device ID.
     */
    bool closeCamera(const std::string &deviceId);

    /**
     * @brief Enumerates devices from default systems.
     */
    void enumerateDevicesFromDefault(const std::vector<std::shared_ptr<rcg::System>> &systems);

    /**
     * @brief Enumerates devices from given systems.
     */
    void enumerateDevicesFromSystems(const std::vector<std::shared_ptr<rcg::System>> &systems);

    /**
     * @brief Lists a single camera.
     */
    bool listCamera(std::shared_ptr<Camera> camera);

    /**
     * @brief List of currently open cameras.
     */
    std::list<std::shared_ptr<Camera>> openCamerasList;

    /**
     * @brief Set of available cameras.
     */
    std::set<std::shared_ptr<rcg::Device>> availableCamerasList;
};

#endif // DEVICEMANAGER_HPP
