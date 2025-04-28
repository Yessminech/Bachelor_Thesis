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
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without specific prior written permission.
 *
 * @note
 * DISCLAIMER
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 #include "SystemManager.hpp"
 #include "DeviceManager.hpp"
 #include "NetworkManager.hpp"
 #include "StreamManager.hpp"
 #include "GlobalSettings.hpp"
 #include "Camera.hpp"
 
 #include <csignal>
 #include <iostream>
 #include <list>
 #include <memory>
 #include <string>
 #include <chrono>
 #include <stdexcept>
 #include <atomic>

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

SystemManager::SystemManager()
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

SystemManager::~SystemManager()
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

void SystemManager::enumerateCameras()
{
    try
    {
        deviceManager.getAvailableCameras();
        availableCamerasIds = deviceManager.listAvailableCamerasByID();

        if (availableCamerasIds.empty())
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

void SystemManager::enumerateOpenCameras(std::list<std::string> camerasIDs)
{
    try
    {
        deviceManager.createCameras(camerasIDs);
        deviceManager.listopenCameras();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error listing available cameras: " << e.what() << std::endl;
        throw;
    }
}
bool SystemManager::setFeature(std::string featureName, std::string value, std::list<std::string> cameraIDs)
{
    try
    {
        if (value.empty())
        {
            throw std::invalid_argument("Value cannot be empty");
        }

        if (!cameraIDs.empty())
        {
            deviceManager.createCameras(cameraIDs);
        }

        for (const auto &cameraID : cameraIDs)
        {
            std::shared_ptr<Camera> camera = deviceManager.getOpenCameraByID(cameraID);
            if (!camera)
            {
                std::cerr << "Camera ID not found in open cameras: " << cameraID << std::endl;
                throw std::runtime_error("Camera ID not found in open cameras.");
            }

            if (featureName == "pixel_format")
            {
                camera->setPixelFormat(value);
            }
            else if (featureName == "exposure_time")
            {
                camera->setExposureTime(std::stod(value));
            }
            else if (featureName == "gain")
            {
                camera->setGain(std::stof(value));
            }
            else if (featureName == "width")
            {
                camera->setWidth(std::stoi(value));
            }
            else if (featureName == "height")
            {
                camera->setHeight(std::stoi(value));
            }
            else
            {
                throw std::invalid_argument("Invalid feature name: " + featureName);
            }
        }

        if (cameraIDs.empty())
        {
            if (featureName == "pixel_format")
            {
                deviceManager.setPixelFormatAll(value);
            }
            else if (featureName == "exposure_time")
            {
                deviceManager.setExposureTimeAll(std::stod(value));
            }
            else
            {
                throw std::invalid_argument("Camera ID is required for setting feature: " + featureName);
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error setting feature: " << e.what() << std::endl;
        return false;
    }
}

void SystemManager::syncFreeRunStream(std::list<std::string> camerasIDs, bool saveStream, std::chrono::milliseconds acquisitionDelay)
{
    try
    {
        std::list<std::shared_ptr<Camera>> openCamerasList = deviceManager.getOpenCameras();

        if (openCamerasList.empty())
        {
            deviceManager.createCameras(camerasIDs);
            openCamerasList = deviceManager.getOpenCameras();

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

bool SystemManager::ptpEnable(std::list<std::string> camerasIDs)
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