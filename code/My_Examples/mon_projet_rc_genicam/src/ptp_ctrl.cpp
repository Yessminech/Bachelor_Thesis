#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>
#include <GenApi/GenApi.h>

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <iomanip>
#include <signal.h>
#include <atomic>
#include <set>

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
};
int ptp_sync_timeout = 800; // ToDo Set or compute this value

// [https://docs.baslerweb.com/timestamp?utm_source=chatgpt.com]
// PTP disabled: 125 MHz (= 125 000 000 ticks per second, 1 tick = 8 ns)
// PTP enabled: 1 GHz (= 1 000 000 000 ticks per second, 1 tick = 1 ns)
// Timestamp could be set

std::atomic<bool> stop_signal(false);

int num_init = 0;
int num_master = 0;
int num_slave = 0;
int64_t master_clock_id = 0;
bool debug = false;

void signalHandler(int signum)
{
    stop_signal = true;
}

bool getGenTLVersion(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    try
    {
        bool deprecatedFeatures = rcg::getString(nodemap, "PtpEnable").empty();
        if (debug)
        {
            std::cout << GREEN << "Version Check success" << RESET << std::endl;
        }
        return deprecatedFeatures;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get GenTL version: " << ex.what() << RESET << std::endl;
        return false;
    }
}

// ToDo add a ptpcheck method that checks if ptp is true on all devices and there are masters and slave it 
void enablePTP(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures)
{
    std::string feature = deprecatedFeatures ? "GevIEEE1588" : "PtpEnable";

    try
    {
        rcg::setString(nodemap, feature.c_str(), "true", true);
        ptpConfig.enabled = rcg::getBoolean(nodemap, feature.c_str());
        if (debug)
        {
            std::cout << GREEN << "PTP enable success" << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to enable PTP: " << ex.what() << RESET << std::endl;
    }
}

void getPtpParameters(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures)
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
        if (debug)
        {
            std::cout << GREEN << "PTP Parameters success " << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get PTP parameters: " << ex.what() << RESET << std::endl;
    }
}
void getTimestamps(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures)
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
        if (debug)
        {
            std::cout << GREEN << "Timestamp success" << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get timestamp: " << ex.what() << RESET << std::endl;
    }
}

void printPtpConfig(PTPConfig ptpConfig)
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
    if (current_status == "Initializing")
    {
        num_init++;
    }
    else if (current_status == "Master")
    {
        num_master++;
    }
    else if (current_status == "Slave")
    {
        num_slave++;
    }
    else
    {
        std::cerr << RED << "Unknown PTP status: " << current_status << RESET << std::endl;
    }
}

void monitorPtpStatus(std::shared_ptr<rcg::Interface> interf, int deviceCount)
{

    auto start_time = std::chrono::steady_clock::now();
    while (!stop_signal && num_slave != deviceCount - 1 && num_master != 1)
    {    int num_init = 0;
        int num_master = 0;
        int num_slave = 0;
        for (auto &device : interf->getDevices())
        {
            if (device->getVendor() != interf->getParent()->getVendor())
            {
                continue;
            }
            device->open(rcg::Device::CONTROL);
            PTPConfig ptpConfig;
            std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();
            bool deprecatedFeatures = getGenTLVersion(nodemap);
            getPtpParameters(nodemap, ptpConfig, deprecatedFeatures);
            statusCheck(ptpConfig.status);
            device->close();
        }
        std::cout << "Camera PTP status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(ptp_sync_timeout))
        {
            std::cerr << "Timed out waiting for camera clocks to become PTP camera slaves. Current status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
            break;
        }
        if (stop_signal)
        {
            break; //cleanup?
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

int main(int argc, char *argv[])
{
    int deviceCount = 0;
    signal(SIGINT, signalHandler);
    std::set<std::string> printedSerialNumbers;
    try
    {
        std::vector<std::shared_ptr<rcg::System>> systems = rcg::System::getSystems();
        if (systems.empty())
        {
            std::cerr << RED << "Error: No systems found." << RESET << std::endl;
            return 1;
        }
        for (auto &system : systems)
        {
            system->open();
            // std::cout << "System path:" << system->getPathname() << std::endl;
            std::vector<std::shared_ptr<rcg::Interface>> interfs = system->getInterfaces();
            if (interfs.empty())
            {
                continue;
            }
            for (auto &interf : interfs)
            {
                interf->open();
                std::vector<std::shared_ptr<rcg::Device>> devices = interf->getDevices();
                if (devices.empty())
                {
                    continue;
                }
                for (auto &device : devices)
                {
                    if (debug)
                    {
                        std::cout << "Device Vendor: " << device->getVendor() << std::endl;
                        std::cout << "System Vendor: " << system->getVendor() << std::endl;
                    }
                    if (device->getVendor() != system->getVendor())
                    {
                        continue;
                    }
                    std::string serialNumber = device->getSerialNumber();
                    if (printedSerialNumbers.find(serialNumber) != printedSerialNumbers.end())
                    {
                        continue;
                    }
                    device->open(rcg::Device::CONTROL);
                    printedSerialNumbers.insert(serialNumber);
                    deviceCount++;
                    PTPConfig ptpConfig;
                    std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();
                    bool deprecatedFeatures = getGenTLVersion(nodemap);
                    if (deprecatedFeatures)
                    {
                        std::cout << YELLOW << "Warning: The device " << device->getID() << " uses deprecated GenTL features." << RESET << std::endl;
                    }
                    enablePTP(nodemap, ptpConfig, deprecatedFeatures);
                    getPtpParameters(nodemap, ptpConfig, deprecatedFeatures);
                    getTimestamps(nodemap, ptpConfig, deprecatedFeatures);
                    if (debug)
                    {
                        std::cout << std::endl;
                        std::cout << GREEN << "Device ID:                  " << device->getID() << RESET << std::endl;
                        printPtpConfig(ptpConfig);
                    }
                    device->close();
                }
                interf->close();
            }
            system->close();
        }
    }

    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
        return 2;
    }

    try
    {
        const char *defaultCtiPath = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib"; // ToDo add other path mvImpact
        if (defaultCtiPath == nullptr)
        {
            std::cerr << RED << "Environment variable GENICAM_GENTL64_PATH is not set." << RESET << std::endl;
            return 1;
        }
        rcg::System::setSystemsPath(defaultCtiPath, nullptr);
        std::vector<std::shared_ptr<rcg::System>> defaultSystems = rcg::System::getSystems();
        if (defaultSystems.empty())
        {
            std::cerr << RED << "Error: No systems found." << RESET << std::endl;
            return 1;
        }
        for (auto &system : defaultSystems)
        {
            system->open();
            std::vector<std::shared_ptr<rcg::Interface>> interfs = system->getInterfaces();
            if (interfs.empty())
            {
                continue;
            }
            for (auto &interf : interfs)
            {
                interf->open();
                std::vector<std::shared_ptr<rcg::Device>> devices = interf->getDevices();
                if (devices.empty())
                {
                    continue;
                }

                for (auto &device : devices)
                {
                    std::string serialNumber = device->getSerialNumber();

                    if (printedSerialNumbers.find(serialNumber) != printedSerialNumbers.end())
                    {
                        continue;
                    }
                    printedSerialNumbers.insert(serialNumber);
                    device->open(rcg::Device::CONTROL);
                    deviceCount++;
                    PTPConfig ptpConfig;
                    auto nodemap = device->getRemoteNodeMap();
                    bool deprecatedFeatures = getGenTLVersion(nodemap);
                    if (deprecatedFeatures)
                    {
                        std::cout << YELLOW << "Warning: The device " << device->getID() << " uses deprecated GenTL features." << RESET << std::endl;
                    }
                    enablePTP(nodemap, ptpConfig, deprecatedFeatures);
                    getPtpParameters(nodemap, ptpConfig, deprecatedFeatures);
                    getTimestamps(nodemap, ptpConfig, deprecatedFeatures);
                    if (debug)
                    {
                        std::cout << std::endl;
                        std::cout << GREEN << "Device ID:                  " << device->getID() << RESET << std::endl;
                        printPtpConfig(ptpConfig);
                    }
                    device->close();
                }
                interf->close();
            }
            system->close();
        }
    }
    catch (const std::exception &ex)
    {
        std::cout << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
    }
    if (deviceCount == 0)
    {
        std::cout << RED << "Warning: No cameras found." << RESET << std::endl;
    }
    else
    {
        std::cout << GREEN << deviceCount << " Cameras configured, checking status.." << RESET << std::endl;
        try
        {
            std::vector<std::shared_ptr<rcg::System>> systems = rcg::System::getSystems();
            if (systems.empty())
            {
                std::cerr << RED << "Error: No systems found." << RESET << std::endl;
                return 1;
            }
            for (auto &system : systems)
            {
                system->open();
                std::vector<std::shared_ptr<rcg::Interface>> interfs = system->getInterfaces();
                if (debug)
                {
                    std::cout << "System path:" << system->getPathname() << std::endl;
                    std::cout << "Number of interfaces: " << interfs.size() << std::endl;
                }
                if (interfs.empty())
                {
                    continue;
                }
                for (auto &interf : interfs)
                {
                    interf->open();
                    if (interf->getDevices().empty())
                    {
                        continue;
                    }
                    monitorPtpStatus(interf, deviceCount);
                    if (num_slave == deviceCount - 1 && num_master == 1)
                    {
                        std::cout << "All camera clocks are PTP slaves to master clock: " << master_clock_id << std::endl;
                        return true;
                    }
                    else
                    {
                        std::cout << "Camera PTP status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
                    }
                    std::vector<std::shared_ptr<rcg::Device>> devices = interf->getDevices();
                    if (devices.empty())
                    {
                        continue;
                    }
                    for (auto &device : devices)
                    {
                        device->open(rcg::Device::CONTROL);
                        std::cout << std::endl;
                        std::cout << GREEN << "Device ID:                  " << device->getID() << RESET << std::endl;
                        PTPConfig ptpConfig;
                        auto nodemap = device->getRemoteNodeMap();
                        bool deprecatedFeatures = getGenTLVersion(nodemap);
                        enablePTP(nodemap, ptpConfig, deprecatedFeatures);
                        getPtpParameters(nodemap, ptpConfig, deprecatedFeatures);
                        getTimestamps(nodemap, ptpConfig, deprecatedFeatures);
                        printPtpConfig(ptpConfig);
                        device->close();
                    }
                    interf->close();
                }
                system->close();
            }
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
            return 2;
        }
    }

    return 0;
}
//ToDo check Grandmaster clock ID?