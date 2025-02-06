#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>

int main(int argc, char *argv[])
{
    int ret = 0;
    std::string feature = "PtpEnable";
    std::string value = "true";

    try
    {
        std::vector<std::shared_ptr<rcg::System>> system = rcg::System::getSystems();

        for (size_t i = 0; i < system.size(); i++)
        {
            system[i]->open();
            std::vector<std::shared_ptr<rcg::Interface>> interf = system[i]->getInterfaces();
            for (size_t k = 0; k < interf.size(); k++)
            {
                interf[k]->open();
                std::vector<std::shared_ptr<rcg::Device>> device = interf[k]->getDevices();
                for (size_t j = 0; j < device.size(); j++)
                {
                    // Open the device in CONTROL mode to access its remote nodemap.
                    device[j]->open(rcg::Device::CONTROL);
                    std::shared_ptr<GenApi::CNodeMapRef> nodemap = device[j]->getRemoteNodeMap();
                    try
                    {
                        rcg::setString(nodemap, feature.c_str(), value.c_str(), true);
                    }
                    catch (const std::exception &ex)
                    {
                        feature = "GevIEEE1588"; // ToDo Change this to be directly read from .xml
                        try
                        {
                            rcg::setString(nodemap, feature.c_str(), value.c_str(), true);
                            std::cout << "Feature '" << feature << "' on device '" << device[j]->getID() << "' is now set to: " << rcg::getBoolean(nodemap, feature.c_str()) << std::endl;
                        }
                        catch (const std::exception &ex)
                        {
                            std::cerr << "Exception: " << ex.what() << std::endl;
                        }
                    }
                    std::cout << "Feature '" << feature << "' on device '" << device[j]->getID() << "' is now set to: " << rcg::getBoolean(nodemap, feature.c_str()) << std::endl;
                    // Close the device.
                    device[j]->close();
                }
                interf[k]->close();
            }
            system[i]->close();
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 2;
    }

    // Clean up the GenICam systems.
    rcg::System::clearSystems();

    return 0;
}


        //     try
        //     {
        //       // just test if Ptp parameters are available
        //       rcg::getString(nodemap, "PtpEnable", true);

        //       if (!readonly)
        //       {
        //         rcg::callCommand(nodemap, "PtpDataSetLatch");

        //         std::cout << "PTP:                        " << rcg::getString(nodemap, "PtpEnable") << std::endl;
        //         std::cout << "PTP status:                 " << rcg::getString(nodemap, "PtpStatus") << std::endl;
        //         std::cout << "PTP offset:                 " << rcg::getInteger(nodemap, "PtpOffsetFromMaster") << " ns" << std::endl;
        //       }
        //       else
        //       {
        //         std::cout << "Ptp cannot be shown due to another application with control access." << std::endl;
        //         std::cout << std::endl;
        //       }
        //     }
        //     catch (const std::exception &)
        //     {
        //       std::cout << "Ptp parameters are not available" << std::endl;
        //       std::cout << std::endl;
        //     }
        //   }

        //   dev->close();
        // }