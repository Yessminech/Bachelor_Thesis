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
            getPTPConfig(nodemap, ptpConfig, deprecatedFeatures);
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

void configureActionCommandInterface(std::shared_ptr<rcg::Interface> interf, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, std::string triggerSource = "Action1", uint32_t actionSelector = 1, uint32_t destinationIP = 0xFFFFFFFF)
{

    try
    {
        auto nodemap = interf->getNodeMap();
        rcg::setInteger(nodemap, "GevActionDestinationIPAddress", destinationIP);
        // Set Action Device Key, Group Key, and Group Mask
        rcg::setInteger(nodemap, "ActionDeviceKey", actionDeviceKey);
        rcg::setInteger(nodemap, "ActionGroupKey", groupKey);
        rcg::setInteger(nodemap, "ActionGroupMask", groupMask);
        rcg::setBoolean(nodemap, "ActionScheduledTimeEnable", true);
        // ✅ Calculate the execution time (e.g., 5 seconds from now)
        auto now = std::chrono::system_clock::now().time_since_epoch();
        uint64_t currTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        uint64_t executeTime = currTimeNs + 5000000000ULL; // 5 seconds later
        rcg::setInteger(nodemap, "ActionScheduledTime", executeTime);

        std::cout << "✅ Interface configured to start acquisition on Action Command.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Failed to configure Action Command Interface: " << e.what() << std::endl;
    }
}

void sendActionCommand(std::shared_ptr<rcg::System> system)
{

    // Fire the Action Command
    try
    {
        auto nodemap = system->getNodeMap();

        rcg::callCommand(nodemap, "ActionCommand");
            std::cout << "✅ Action Command scheduled \n"; // for execution at: " << executeTime << " ns.

    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Failed to send Action Command: " << e.what() << std::endl;
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