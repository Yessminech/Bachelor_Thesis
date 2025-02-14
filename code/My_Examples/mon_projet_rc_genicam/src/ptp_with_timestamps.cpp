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

// Global variables
std::atomic<bool> stop_program(false);

// ANSI color codes
#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"

int num_init = 0;
int num_master = 0;
int num_slave = 0;
int offset_threshold = 30;    // ns
int accuracy_threshhold = 30; // ns

void signalHandler(int signum)
{
    std::cout << "\nStopping program..." << std::endl;
    stop_program = true;
}

// ToDo Correct this 
void cleanup(std::vector<std::shared_ptr<rcg::System>>& systems)
{
    for (auto &system : systems)
    {
        for (auto &interf : system->getInterfaces())
        {
            for (const auto &device : interf->getDevices())
            {
                // if (device->isOpen())
                {
                    device->close();
                }
            }
            // if (interf->isOpen())
            {
                interf->close();
            }
        }
        // if (system->isOpen())
        {
            system->close();
        }
    }
    rcg::System::clearSystems();
}
void enablePTP(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    std::string feature = "PtpEnable";
    std::string value = "true";
    try
    {
        rcg::setString(nodemap, feature.c_str(), value.c_str(), true);
    }
    catch (const std::exception &ex)
    {
        // std::cerr << YELLOW << "Warning: Failed to set " << feature << ": - Trying deprecated feature: GevIEEE1588" << RESET << std::endl;
        feature = "GevIEEE1588";
        try
        {
            rcg::setString(nodemap, feature.c_str(), value.c_str(), true);
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Error: Failed to set " << feature << ": " << ex.what() << RESET << std::endl;
        }
    }
}

void printTimestamp(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    if (rcg::getString(nodemap, "TimestampLatchValue") == "")
    {
        // std::cerr << YELLOW << "Warning: Failed to get Timestamp from device '" << deviceID << "': - Trying deprecated features: GevTimestampControlLatch, GevTimestampValue" << RESET << std::endl;

        rcg::callCommand(nodemap, "GevTimestampControlLatch");
        uint64_t timestamp = std::stoull(rcg::getString(nodemap, "GevTimestampValue"));
        double tickFrequency = std::stod(rcg::getString(nodemap, "GevTimestampTickFrequency")); // if PTP is used, this feature must return 1,000,000,000 (1 GHz).
        uint64_t timestamp_s = timestamp / tickFrequency;
        std::cout << "Timestamp:                  " << timestamp << " ns" << std::endl;
    }
    else
    {
        std::cout << "Timestamp:                  " << rcg::getString(nodemap, "TimestampLatchValue") << " ns" << std::endl;
    }
}

void printPtpConfig(std::shared_ptr<GenApi::CNodeMapRef> nodemap, const std::string &deviceID)
{
    try
    {
        rcg::getString(nodemap, "PtpEnable", true);
        rcg::callCommand(nodemap, "TimestampLatch");
        rcg::callCommand(nodemap, "PtpDataSetLatch");
        std::cout << GREEN << "Device ID:                  " << deviceID << RESET << std::endl;
        std::cout << "PTP:                        " << rcg::getString(nodemap, "PtpEnable") << std::endl;
        std::cout << "PTP status:                 " << rcg::getString(nodemap, "PtpStatus") << std::endl;
        std::cout << "Clock accuray:              " << rcg::getString(nodemap, "PtpClockAccuracy") << std::endl;
        std::cout << "PTP offset:                 " << rcg::getInteger(nodemap, "PtpOffsetFromMaster") << " ns" << std::endl;
        printTimestamp(nodemap);
    }
    catch (const std::exception &ex)
    {
        // std::cerr << YELLOW << "Warning: Failed to get PTP configuration from device '" << deviceID << "'- Trying deprecated features: GevIEEE1588, GevIEEE1588Status " << RESET << std::endl;
        try
        {
            rcg::getString(nodemap, "GevIEEE1588", true);
            std::cout << GREEN << "Device ID:                  " << deviceID << RESET << std::endl;
            std::cout << "PTP:                        " << rcg::getString(nodemap, "GevIEEE1588") << std::endl;
            std::cout << "PTP status:                 " << rcg::getString(nodemap, "GevIEEE1588Status") << std::endl;
            // std::cout << "Clock accuracy              " << rcg::getEnum(nodemap, "GevIEEE1588ClockAccuracy") << std::endl; //ToDo Invisible?
            printTimestamp(nodemap);
        }
        catch (const std::exception &ex)
        {
            std::cout << RED << "Ptp parameters are not available" << std::endl;
            std::cout << std::endl;
        }
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
void setThreshhold(std::shared_ptr<rcg::Interface> interf)
{
    bool slaves = false;
    bool offset_available = false;
    const int timeout_seconds = 10; // Timeout after 10 seconds
    auto start_time = std::chrono::steady_clock::now();

    for (auto &device : interf->getDevices())
    {
        while (!slaves && !stop_program)
        {
            device->open(rcg::Device::CONTROL);
            auto nodemap = device->getRemoteNodeMap();

            std::string status;
            try
            {
                status = rcg::getString(nodemap, "PtpStatus");
                offset_available = true;
            }
            catch (const std::exception &ex)
            {
                // Handle exception if needed
            }

            if (status == "Slave" && offset_available)
            {
                slaves = true;

                do
                {
                    std::cout << "PtpOffsetFromMaster: " << rcg::getInteger(nodemap, "PtpOffsetFromMaster") << std::endl;
                    printTimestamp(nodemap);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep for 100ms to avoid busy waiting
                } while (rcg::getInteger(nodemap, "PtpOffsetFromMaster") > offset_threshold &&
                         std::chrono::steady_clock::now() - start_time < std::chrono::seconds(timeout_seconds) &&
                         !stop_program);
            }

            device->close();

            if (std::chrono::steady_clock::now() - start_time >= std::chrono::seconds(timeout_seconds))
            {
                std::cout << YELLOW << "Warning: Timeout reached while waiting for PTP offset to be within threshold." << RESET << std::endl;
                break;
            }
        }

        // Check if the program should stop
        if (stop_program)
        {
            break;
        }
    }
}
int main(int argc, char *argv[])
{
    // Register signal handler for Ctrl+C
    // signal(SIGINT, signalHandler);

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
                    auto nodemap = device->getRemoteNodeMap();

                    enablePTP(nodemap);
                    printPtpConfig(nodemap, device->getID());
                    std::string current_status = rcg::getString(nodemap, "GevIEEE1588Status");
                    statusCheck(current_status);
                    device->close();

                    // Check if the program should stop
                    if (stop_program)
                    {
                        break;
                    }
                }
                // setThreshhold(interf);
                interf->close();

                // Check if the program should stop
                if (stop_program)
                {
                    break;
                }
            }
            system->close();

            // Check if the program should stop
            if (stop_program)
            {
                break;
            }
        }
        if (deviceCount == 0)
        {
            std::cout << YELLOW << "Warning: No cameras found." << RESET << std::endl;
        }
        else
        {
            // ToDo: Print Status until Master is found
            std::cout << GREEN << "Camera PTP status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << RESET << std::endl;
            std::cout << "Number of devices found: " << deviceCount << std::endl;
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