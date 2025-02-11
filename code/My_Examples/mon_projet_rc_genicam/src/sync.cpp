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

void enablePTP(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    std::string feature = "PtpEnable";
    std::string value = "true";
    try
    {
        rcg::setString(nodemap, feature.c_str(), value.c_str(), true);
        std::cout << "Feature '" << feature << "' is now set to: " << rcg::getBoolean(nodemap, feature.c_str()) << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Failed to set " << feature << ": - Trying depracted feature: GevIEEE1588" << std::endl;
        feature = "GevIEEE1588";
        try
        {
            rcg::setString(nodemap, feature.c_str(), value.c_str(), true);
            std::cout << "Feature '" << feature << "' is now set to: " << rcg::getBoolean(nodemap, feature.c_str()) << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Failed to set " << feature << ": " << ex.what() << std::endl;
        }
    }
}

void printTimestamp(std::shared_ptr<GenApi::CNodeMapRef> nodemap, const std::string &deviceID)
{
    try
    {
        rcg::callCommand(nodemap, "TimestampLatch");
        if (rcg::getString(nodemap, "TimestampLatchValue") == "")
        {
            std::cerr << "Failed to get Timestamp from device '" << deviceID << ": - Trying depracted features: GevTimestampControlLatch, GevTimestampValue" << std::endl;

            rcg::callCommand(nodemap, "GevTimestampControlLatch");
            uint64_t timestamp = std::stoull(rcg::getString(nodemap, "GevTimestampValue"));
            double tickFrequency = std::stod(rcg::getString(nodemap, "GevTimestampTickFrequency")); // if PTP is used, this feature must return 1,000,000,000 (1 GHz).
            uint64_t timestamp_s = timestamp / tickFrequency;
            std::cout << "Timestamp on device '" << deviceID << "' is: " << timestamp_s << std::endl;
        }

        else
        {
            std::cout << "Timestamp on device '" << deviceID << "' is: " << rcg::getString(nodemap, "TimestampLatchValue") << "ns." << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Failed to get Timestamp from device '" << deviceID << ": - Trying depracted features: GevTimestampControlLatch, GevTimestampValue" << std::endl;
        try
        {
            rcg::callCommand(nodemap, "GevTimestampControlLatch");
            std::cout << "Timestamp on device '" << deviceID << "' is: " << rcg::getString(nodemap, "GevTimestampValue") << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Failed to get Timestamp from device '" << deviceID << ": " << ex.what() << std::endl;
        }
    }
}

void printPtpConfig(std::shared_ptr<GenApi::CNodeMapRef> nodemap, const std::string &deviceID)
{

    try
    {
        rcg::getString(nodemap, "PtpEnable", true);
        rcg::callCommand(nodemap, "PtpDataSetLatch");
        std::cout << "PTP:                        " << rcg::getString(nodemap, "PtpEnable") << std::endl;
        std::cout << "PTP status:                 " << rcg::getString(nodemap, "PtpStatus") << std::endl;
        std::cout << "PTP offset:                 " << rcg::getInteger(nodemap, "PtpOffsetFromMaster") << " ns" << std::endl;
    }
    catch (const std::exception &)
    {
        std::cerr << "Failed to get PTP configuration from device '" << deviceID << "'- Trying depracted features: GevIEEE1588, GevIEEE1588Status " << std::endl;
        try
        {
            rcg::getString(nodemap, "GevIEEE1588", true);
            std::cout << "PTP:                        " << rcg::getString(nodemap, "GevIEEE1588") << std::endl;
            std::cout << "PTP status:                 " << rcg::getString(nodemap, "GevIEEE1588Status") << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cout << "Ptp parameters are not available" << std::endl;
            std::cout << std::endl;
        }
    }
}

// Waits until all cameras are set to slaves and one camera to master
bool waitForPTPStatus(const std::vector<std::shared_ptr<rcg::Device>> &cameras, bool deprecated, int ptp_sync_timeout)
{
    std::string feature;
    std::cout << "Waiting for " << cameras.size() << " cameras to have correct PTP status." << std::endl;
    if (deprecated)
    {
        feature = "GevIEEE1588Status";
    }
    else
    {
        feature = "PtpStatus";
    }
    auto timeout_time = std::chrono::steady_clock::now() + std::chrono::seconds(ptp_sync_timeout);
    while (true)
    {
        int num_init = 0;
        int num_master = 0;
        int num_slave = 0;
        int64_t master_clock_id = 0;

        for (const auto &camera : cameras)
        {
            auto nodemap = camera->getRemoteNodeMap();
            std::string current_status = rcg::getString(nodemap, feature.c_str());

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

        if (num_slave == cameras.size() - 1 && num_master == 1)
        {
            std::cout << "All camera clocks are PTP slaves to master clock: " << master_clock_id << std::endl;
            return true;
        }
        else
        {
            std::cout << "Camera PTP status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
        }

        if (std::chrono::steady_clock::now() > timeout_time)
        {
            std::cerr << "Timed out waiting for camera clocks to become PTP camera slaves. Current status: " << num_init << " initializing, " << num_master << " masters, " << num_slave << " slaves" << std::endl;
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// ToDo: Double read this function
//  Waits until all cameras maintain a clock offset below max_offset_ns for offset_window_s seconds.
// bool waitForPTPClockSync(const std::vector<std::shared_ptr<rcg::Device>> &cameras, int max_offset_ns, int offset_window_s, int ptp_sync_timeout)
// {
//     std::cout << "Waiting for clock offsets < " << max_offset_ns << " ns over " << offset_window_s << " s..." << std::endl;
//     bool currently_below = false;
//     auto below_start = std::chrono::steady_clock::now();
//     std::vector<int64_t> current_offsets(cameras.size(), 0);
//     int64_t current_max_abs_offset = 0;

//     auto timeout_time = std::chrono::steady_clock::now() + std::chrono::seconds(ptp_sync_timeout);
//     while (true)
//     {
//         current_max_abs_offset = 0;
//         for (size_t i = 0; i < cameras.size(); ++i)
//         {
//             cameras[i]->open(rcg::Device::CONTROL);
//             auto nodemap = cameras[i]->getRemoteNodeMap();
//             if (nodemap)
//             {
//                 auto latchNode = nodemap->GetNode("GevIEEE1588DataSetLatch");
//                 if (latchNode)
//                     latchNode->Execute();
//                 try
//                 {
//                 }
//                 catch (const std::exception &ex)
//                 {
//                     std::cerr << "Failed to get PTP offset from device '" << cameras[i]->getID() << "', trying deprecated features " << std::endl;
//                 }
//                 auto offsetNode = nodemap->GetNode("GevIEEE1588OffsetFromMaster");
//                 if (offsetNode)
//                 {
//                     current_offsets[i] = offsetNode->GetValue();
//                     current_max_abs_offset = std::max(current_max_abs_offset,
//                                                       static_cast<int64_t>(std::abs(current_offsets[i])));
//                 }
//             }
//             cameras[i]->close();
//         }
//         if (current_max_abs_offset < max_offset_ns)
//         {
//             if (!currently_below)
//             {
//                 currently_below = true;
//                 below_start = std::chrono::steady_clock::now();
//             }
//             else if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - below_start).count() >= offset_window_s)
//             {
//                 std::cout << "All clocks synced" << std::endl;
//                 return true;
//             }
//         }
//         else
//         {
//             currently_below = false;
//         }
//         if (std::chrono::steady_clock::now() > timeout_time)
//         {
//             std::cerr << "PTP clock synchronization timed out waiting for offsets < " << max_offset_ns << " ns over " << offset_window_s << " s" << std::endl;
//             std::cerr << "Current clock offsets:" << std::endl;
//             for (size_t i = 0; i < cameras.size(); ++i)
//             {
//                 std::cerr << i << ": " << current_offsets[i] << std::endl;
//             }
//             return false;
//         }
//         std::this_thread::sleep_for(std::chrono::milliseconds(500));
//     }
//     return false;
// }

int main(int argc, char *argv[])
{
    try
    {
        std::vector<std::shared_ptr<rcg::System>> systems = rcg::System::getSystems();
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
                    printTimestamp(nodemap, device->getID());
                    printPtpConfig(nodemap, device->getID());
                    if (!waitForPTPStatus({device}, true, 10)) //ToDo set right value for timeout and change deprecated implementation
                    {
                        return 1;
                    }
                    device->close();
                }
                interf->close();
            }
            system->close();
        }
        if (deviceCount == 0)
        {
            std::cout << "No cameras found." << std::endl;
        }
        else
        {
            std::cout << "Number of devices found: " << deviceCount << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 2;
    }

    rcg::System::clearSystems();
    return 0;
}
// if (!waitForPTPSlave(cameras) || !waitForPTPClockSync(cameras, ptp_max_offset_ns, ptp_offset_window_s))
// {
//     return 1;
// }
