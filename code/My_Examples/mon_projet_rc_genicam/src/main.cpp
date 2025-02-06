#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>

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
        rcg::callCommand(nodemap, "TimestampReset");
        if (rcg::getString(nodemap, "TimestampLatchValue") == "")
        {
            std::cerr << "Failed to get Timestamp from device '" << deviceID << ": - Trying depracted features: GevTimestampControlLatch, GevTimestampValue" << std::endl;

            rcg::callCommand(nodemap, "GevTimestampControlLatch");
            std::cout << "Timestamp on device '" << deviceID << "' is: " << rcg::getString(nodemap, "GevTimestampValue") << std::endl;
        }

        else
        {
            std::cout << "Timestamp on device '" << deviceID << "' is: " << rcg::getString(nodemap, "TimestampLatchValue") << std::endl;
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

int main(int argc, char *argv[])
{
    try
    {
        std::vector<std::shared_ptr<rcg::System>> systems = rcg::System::getSystems();
        int deviceCount = 0;
        for (auto& system : systems)
        {
            system->open();
            for (auto& interf : system->getInterfaces())
            {
                interf->open();
                for (auto& device : interf->getDevices())
                {
                    deviceCount++;
                    device->open(rcg::Device::CONTROL);
                    auto nodemap = device->getRemoteNodeMap();

                    enablePTP(nodemap);
                    printTimestamp(nodemap, device->getID());
                    printPtpConfig(nodemap, device->getID());
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
