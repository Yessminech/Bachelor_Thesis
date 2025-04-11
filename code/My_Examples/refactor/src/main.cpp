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
              << "  " << programName << " --list\n"
              << "  " << programName << " --start --cameras ""cam1,cam2,..."" [--delay <ms>] [--no-save]\n"
              << "  " << programName << " --enable-ptp --cameras ""cam1,cam2,...""\n"
              << "  " << programName << " --disable-ptp --cameras ""cam1,cam2,...""\n\n"
              << "Options:\n"
              << "  --list                      List all connected cameras\n"
              << "  --start                     Start synchronized acquisition\n"
              << "  --cameras ""cam1,cam2,...""     Comma-separated list of camera IDs\n"
              << "  --delay ""ms""                Acquisition delay in milliseconds (default: 0)\n"
              << "  --no-save                   Do not save video or PNGs\n"
              << "  --enable-ptp                Enable PTP on selected cameras\n"
              << "  --disable-ptp               Disable PTP on selected cameras\n"
              << "  --help                      Show this help message\n";
            std::cout << "\nExamples:\n"
                      << "  " << programName << " --list\n"
                      << "  " << programName << " --start --cameras \"Basler acA2440-20gc (23630914),Basler acA2440-20gc (23630913),210200799\" --delay 100\n"
                      << "  " << programName << " --start --cameras \"Basler acA2440-20gc (23630914),Basler acA2440-20gc (23630913),210200799\" --no-save\n"
                      << "  " << programName << " --enable-ptp --cameras \"Basler acA2440-20gc (23630914),Basler acA2440-20gc (23630913),210200799\"\n"
                      << "  " << programName << " --disable-ptp --cameras \"Basler acA2440-20gc (23630914),Basler acA2440-20gc (23630913),210200799\"\n";
}

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
    bool saveStream = true;
    std::list<std::string> cameraIDs;
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
        else
        {
            std::cerr << "[ERROR] Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    SystemManager systemManager;

    if (listMode)
    {
        systemManager.initializeSystem();  
        //todo : add a function to list all cameras
        systemManager.shutdownSystem();
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
        systemManager.initializeSystem(); // should be called at start
        systemManager.syncFreeRunStream(cameraIDs, saveStream, acquisitionDelay);
        systemManager.shutdownSystem();
        return 0;
    }

    if (enablePTP || disablePTP)
    {
        if (cameraIDs.empty())
        {
            std::cerr << "[ERROR] --cameras must be specified with --enable-ptp or --disable-ptp.\n";
            return 1;
        }
        systemManager.initializeSystem(); // should be called at start
        bool success = enablePTP
                       ? systemManager.ptpEnable(cameraIDs)
                       : systemManager.ptpDisable(cameraIDs);

        if (!success)
        {
            std::cerr << "[ERROR] Failed to change PTP state on one or more cameras.\n";
            return 1;
        }

        systemManager.shutdownSystem();
        return 0;
    }

    std::cerr << "[ERROR] You must specify --list, --start, --enable-ptp, or --disable-ptp.\n";
    printUsage(argv[0]);
    return 1;
}
