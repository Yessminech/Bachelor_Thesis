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

#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <list>
#include <regex>
#include <chrono>
#include <QApplication>
#include <filesystem>

void printUsage(const std::string &programName)
{
    std::cout << "Usage:\n"
              << "  " << programName << " --list [--cameras \"cam1,cam2,...\"]\n\n"
              << "  " << programName << " --start --cameras \"cam1,cam2,...\" [--delay <ms>] [--no-save]\n"
              << "  " << programName << " --enable-ptp --cameras \"cam1,cam2,...\"\n"
              << "  " << programName << " --disable-ptp --cameras \"cam1,cam2,...\"\n"
              << "  " << programName << " --set-feature --feature <name> --value <value> [--cameras \"cam1,cam2,...\"]\n\n"
              << "Options:\n"
              << "  --list                      List all connected cameras\n"
              << "  --start                     Start synchronized acquisition\n"
              << "  --cameras \"cam1,cam2,...\"     Comma-separated list of camera IDs\n"
              << "  --delay \"ms\"                Acquisition delay in milliseconds (default: 0)\n"
              << "  --no-save                   Do not save video or PNGs\n"
              << "  --enable-ptp                Enable PTP on selected cameras\n"
              << "  --disable-ptp               Disable PTP on selected cameras\n"
              << "  --set-feature               Set a feature on cameras\n"
              << "  --feature <name>            Feature name (width, height, gain, exposure_time, pixel_format)\n"
              << "  --value <value>             Value to set for the feature\n"
              << "  --help                      Show this help message\n";
    std::cout << "\nExamples:\n"
              << "  " << programName << " --list --cameras \"cam1\"\n"
              << "  " << programName << " --start --cameras \"cam1,cam2,...\" --delay 100\n"
              << "  " << programName << " --enable-ptp --cameras \"cam1,cam2,...\"\n"
              << "  " << programName << " --set-feature --feature width --value 1920 --cameras \"cam1,cam2\"\n"
              << "  " << programName << " --set-feature --feature exposure_time --value 5000\n";
}
// ./cli --start --cameras "Basler acA2440-20gc (23630914),Basler acA2440-20gc (23630913),210200799" --no-save

std::list<std::string> parseCameraIDs(const std::string &input)
{
    std::list<std::string> cameraIDs;
    std::regex re("\\s*,\\s*");
    std::sregex_token_iterator it(input.begin(), input.end(), re, -1), end;
    for (; it != end; ++it)
    {
        if (!it->str().empty())
            cameraIDs.push_back(it->str());
    }
    return cameraIDs;
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    bool listMode = false;
    bool startMode = false;
    bool enablePTP = false;
    bool disablePTP = false;
    bool setFeatureMode = false;
    bool saveStream = true;
    std::list<std::string> cameraIDs;
    std::string featureName;
    std::string featureValue;
    std::chrono::milliseconds acquisitionDelay(0);

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--list")
        {
            listMode = true;
        }
        else if (arg == "--start")
        {
            startMode = true;
        }
        else if (arg == "--cameras" && i + 1 < argc)
        {
            cameraIDs = parseCameraIDs(argv[++i]);
        }
        else if (arg == "--delay" && i + 1 < argc)
        {
            acquisitionDelay = std::chrono::milliseconds(std::stoi(argv[++i]));
        }
        else if (arg == "--no-save")
        {
            saveStream = false;
        }
        else if (arg == "--enable-ptp")
        {
            enablePTP = true;
        }
        else if (arg == "--disable-ptp")
        {
            disablePTP = true;
        }
        else if (arg == "--set-feature")
        {
            setFeatureMode = true;
        }
        else if (arg == "--feature" && i + 1 < argc)
        {
            featureName = argv[++i];
        }
        else if (arg == "--value" && i + 1 < argc)
        {
            featureValue = argv[++i];
        }
        else
        {
            std::cerr << "[ERROR] Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    SystemManager systemManager = SystemManager();

    if (listMode)
    {
        if (cameraIDs.empty())
        {
            std::cerr << "Add --cameras  to get the device specific information.\n";
        }
        else
        {
            systemManager.enumerateOpenCameras(cameraIDs);
        }
        return 0;
    }

    if (startMode)
    {
        if (cameraIDs.empty())
        {
            std::cerr << "[ERROR] --cameras must be specified in --start mode.\n";
            printUsage(argv[0]);
            return 1;
        }
        systemManager.syncFreeRunStream(cameraIDs, saveStream, acquisitionDelay);
        return 0;
    }

    if (enablePTP || disablePTP)
    {
        if (cameraIDs.empty())
        {
            std::cerr << "[ERROR] --cameras must be specified with --enable-ptp or --disable-ptp.\n";
            return 1;
        }
        bool success = enablePTP
                           ? systemManager.ptpEnable(cameraIDs)
                           : systemManager.ptpDisable(cameraIDs);

        if (!success)
        {
            std::cerr << "[ERROR] Failed to change PTP state on one or more cameras.\n";
            return 1;
        }
        return 0;
    }

    if (setFeatureMode)
    {
        if (featureName.empty() || featureValue.empty())
        {
            std::cerr << "[ERROR] --feature and --value must be specified in --set-feature mode.\n";
            printUsage(argv[0]);
            return 1;
        }

        if (cameraIDs.empty())
        {
            if (!systemManager.setFeature(featureName, featureValue))
            {
                std::cerr << "[ERROR] Failed to set global feature: " << featureName << ".\n";
                return 1;
            }
        }
        else
        {
            if (!systemManager.setFeature(featureName, featureValue, cameraIDs))
            {
                std::cerr << "[ERROR] Failed to set feature: " << featureName
                          << ".\n";
                return 1;
            }
        }

        return 0;
    }

    std::cerr << "[ERROR] You must specify --list, --start, --enable-ptp, --disable-ptp, or --set-feature.\n";
    printUsage(argv[0]);
    return 1;
}
