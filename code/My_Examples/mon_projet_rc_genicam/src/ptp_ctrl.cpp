#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <iomanip>
#include <signal.h>
#include <atomic>

// ANSI color codes
#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"
struct PTPConfig
{
    bool enabled;
    std::string status;
    std::string timestamp_ns;
    std::string tickFrequency;
    uint64_t timestamp_s;
    std::string clockAccuracy;
    int offsetFromMaster;
    int threshhold; // ToDo set or compute this
};

// [https://docs.baslerweb.com/timestamp?utm_source=chatgpt.com]
// PTP disabled: 125 MHz (= 125 000 000 ticks per second, 1 tick = 8 ns)
// PTP enabled: 1 GHz (= 1 000 000 000 ticks per second, 1 tick = 1 ns)
// Timestamp could be set 

std::atomic<bool> stop_program(false);
int num_init = 0;
int num_master = 0;
int num_slave = 0;

bool getGenTLVersion(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    try
    {
        bool deprecatedFeatures = rcg::getString(nodemap, "GevIEEE1588") != "" ? false : true;
        if (deprecatedFeatures)
        {
            std::cout << YELLOW << "Warning: The device uses deprecated GenTL features." << RESET << std::endl;
        }
        return deprecatedFeatures;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get GenTL version: " << ex.what() << RESET << std::endl;
        return false;
    }
}

void signalHandler(int signum)
{
    std::cout << "\nStopping program..." << std::endl;
    stop_program = true;
}

void enablePTP(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig ptpConfig, bool deprecatedFeatures)
{
    std::string feature = deprecatedFeatures ? "GevIEEE1588" : "PtpEnable";

    try
    {
        rcg::setString(nodemap, feature.c_str(), "true", true);
        ptpConfig.enabled = rcg::getBoolean(nodemap, feature.c_str());
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to enable PTP: " << ex.what() << RESET << std::endl;
    }
}

void getPtpParameters(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig ptpConfig, bool deprecatedFeatures)
{
    try
    {
        if (!deprecatedFeatures)
        {
            rcg::callCommand(nodemap, "TimestampLatch");
            rcg::callCommand(nodemap, "PtpDataSetLatch");
            ptpConfig.enabled = rcg::getBoolean(nodemap, "PtpEnable");
            ptpConfig.status = rcg::getString(nodemap, "PtpStatus");
            ptpConfig.clockAccuracy = rcg::getString(nodemap, "PtpClockAccuracy");
            ptpConfig.offsetFromMaster = rcg::getInteger(nodemap, "PtpOffsetFromMaster");
        }
        else
        {
            rcg::callCommand(nodemap, "GevIEEE1588");
            ptpConfig.enabled = rcg::getBoolean(nodemap, "GevIEEE1588");
            ptpConfig.status = rcg::getString(nodemap, "GevIEEE1588Status");
            ptpConfig.clockAccuracy = "Unavailable";
            ptpConfig.offsetFromMaster = -1;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get PTP parameters: " << ex.what() << RESET << std::endl;
    }
}
void getTimestamps(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig ptpConfig, bool deprecatedFeatures)
{
    try
    {
        if (!deprecatedFeatures)
        {
            rcg::callCommand(nodemap, "TimestampLatch");
            ptpConfig.timestamp_ns = rcg::getString(nodemap, "TimestampLatchValue");
            ptpConfig.tickFrequency = "1000000000"; // Assuming 1 GHz if PTP is used
        }
        else
        {
            rcg::callCommand(nodemap, "GevTimestampControlLatch");
            ptpConfig.timestamp_ns = rcg::getString(nodemap, "GevTimestampValue");
            ptpConfig.tickFrequency = rcg::getString(nodemap, "GevTimestampTickFrequency"); // if PTP is used, this feature must return 1,000,000,000 (1 GHz).
        }

        ptpConfig.timestamp_s = std::stoull(ptpConfig.timestamp_ns) / std::stoull(ptpConfig.tickFrequency);
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get timestamp: " << ex.what() << RESET << std::endl;
    }
}

void printPtpConfig(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig ptpConfig)
{
    try
    {

        std::cout << "PTP Enabled:                " << (ptpConfig.enabled ? "Yes" : "No") << std::endl;
        std::cout << "PTP Status:                 " << ptpConfig.status << std::endl;
        std::cout << "PTP Clock Accuracy:         " << ptpConfig.clockAccuracy << std::endl;
        std::cout << "PTP Offset From Master:     " << ptpConfig.offsetFromMaster << " ns" << std::endl;
        std::cout << "Timestamp:                  " << ptpConfig.timestamp_ns << " ns" << std::endl;
        std::cout << "Timestamp (seconds):        " << ptpConfig.timestamp_s << " s" << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get timestamp: " << ex.what() << RESET << std::endl;
    }
}

void statusCheck(const std::string &current_status)
{
    if (current_status == "Master")
    {
        ++num_master;
    }
    else if (current_status == "Slave")
    {
        ++num_slave;
    }
    else
    {
        ++num_init;
    }
}

void monitorPtpStatus(std::shared_ptr<rcg::Interface> interf, int deviceCount)
{
    while (num_slave != deviceCount - 1 && num_master != 1)
    {
        for (auto &device : interf->getDevices())
        {
            device->open(rcg::Device::CONTROL);
            PTPConfig ptpConfig;
            auto nodemap = device->getRemoteNodeMap();
            bool deprecatedFeatures = getGenTLVersion(nodemap);
            getPtpParameters(nodemap, ptpConfig, deprecatedFeatures);
            std::cout << "Camera PTP status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            device->close();
        }
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler);
    std::vector<std::shared_ptr<rcg::System>> systems;

    try
    {
        systems = rcg::System::getSystems();
        int deviceCount = 0;
        for (auto &system : systems)
        {
            system->open();
            for (auto &interf : system->getInterfaces())
            {
                interf->open();
                for (auto &device : interf->getDevices())
                {
                    deviceCount++;
                    device->open(rcg::Device::CONTROL);
                    PTPConfig ptpConfig;
                    auto nodemap = device->getRemoteNodeMap();
                    bool deprecatedFeatures = getGenTLVersion(nodemap);
                    enablePTP(nodemap, ptpConfig, deprecatedFeatures);
                    getPtpParameters(nodemap, ptpConfig, deprecatedFeatures);
                    getTimestamps(nodemap, ptpConfig, deprecatedFeatures);
                    std::cout << GREEN << "Device ID:                  " << device->getID() << RESET << std::endl;
                    printPtpConfig(nodemap, ptpConfig);
                    statusCheck(ptpConfig.status);
                    device->close();
                    if (stop_program)
                    {
                        break;
                    }
                }
                monitorPtpStatus(interf, deviceCount);
                std::cout << GREEN << "Camera PTP status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << RESET << std::endl;

                interf->close();

                if (stop_program)
                {
                    break;
                }
            }
            system->close();

            if (stop_program)
            {
                break;
            }
        }

        if (deviceCount == 0)
        {
            std::cout << RED << "Warning: No cameras found." << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
        // cleanup(systems);
        return 2;
    }

    // cleanup(systems);
    return 0;
}

// // ToDo Correct this
// void cleanup(std::vector<std::shared_ptr<rcg::System>> &systems)
// {
//     for (auto &system : systems)
//     {
//         for (auto &interf : system->getInterfaces())
//         {
//             for (const auto &device : interf->getDevices())
//             {
//                 // if (device->isOpen())
//                 {
//                     device->close();
//                 }
//             }
//             // if (interf->isOpen())
//             {
//                 interf->close();
//             }
//         }
//         // if (system->isOpen())
//         {
//             system->close();
//         }
//     }
//     rcg::System::clearSystems();
// }

// void setThreshhold(std::shared_ptr<rcg::Interface> interf)
// {
//     bool slaves = false;
//     bool offset_available = false;
//     const int timeout_seconds = 10; // Timeout after 10 seconds
//     auto start_time = std::chrono::steady_clock::now();

//     for (auto &device : interf->getDevices())
//     {
//         while (!slaves && !stop_program)
//         {
//             device->open(rcg::Device::CONTROL);
//             auto nodemap = device->getRemoteNodeMap();

//             std::string status;
//             try
//             {
//                 status = rcg::getString(nodemap, "PtpStatus");
//                 offset_available = true;
//             }
//             catch (const std::exception &ex)
//             {
//                 // Handle exception if needed
//             }

//             if (status == "Slave" && offset_available)
//             {
//                 slaves = true;

//                 do
//                 {
//                     std::cout << "PtpOffsetFromMaster: " << rcg::getInteger(nodemap, "PtpOffsetFromMaster") << std::endl;
//                     printTimestamp(nodemap);
//                     std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep for 100ms to avoid busy waiting
//                 } while (rcg::getInteger(nodemap, "PtpOffsetFromMaster") > offset_threshold &&
//                          std::chrono::steady_clock::now() - start_time < std::chrono::seconds(timeout_seconds) &&
//                          !stop_program);
//             }

//             device->close();

//             if (std::chrono::steady_clock::now() - start_time >= std::chrono::seconds(timeout_seconds))
//             {
//                 std::cout << YELLOW << "Warning: Timeout reached while waiting for PTP offset to be within threshold." << RESET << std::endl;
//                 break;
//             }
//         }

//         // Check if the program should stop
//         if (stop_program)
//         {
//             break;
//         }
//     }
// }
