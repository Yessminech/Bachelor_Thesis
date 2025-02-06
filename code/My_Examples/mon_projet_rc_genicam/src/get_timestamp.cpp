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
                        rcg::callCommand(nodemap, "GevTimestampControlLatch");
                        std::cout
                            << "Timestamp on device '" << device[j]->getID() << "' is : " << rcg::getString(nodemap, "GevTimestampValue") << std::endl;
                    }
                    catch (const std::exception &ex)
                    {
                        std::cout << "Failed to get Timestamp Frequency" << std::endl;
                    }
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
