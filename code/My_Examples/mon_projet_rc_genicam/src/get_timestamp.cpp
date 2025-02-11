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
    try
    {
        // Retrieve all available systems
        std::vector<std::shared_ptr<rcg::System>> systems = rcg::System::getSystems();

        for (const auto& system : systems)
        {
            system->open();
            std::vector<std::shared_ptr<rcg::Interface>> interfaces = system->getInterfaces();

            for (const auto& interf : interfaces)
            {
                interf->open();
                std::vector<std::shared_ptr<rcg::Device>> devices = interf->getDevices();

                for (const auto& device : devices)
                {
                    // Open the device in CONTROL mode to access its remote nodemap
                    device->open(rcg::Device::CONTROL);
                    std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();

                    try
                    {
                        // Execute the Timestamp Latch command
                        rcg::callCommand(nodemap, "GevTimestampControlLatch");

                        // Retrieve the latched timestamp value
                        int64_t timestamp = rcg::getInteger(nodemap, "GevTimestampValue");

                        // Retrieve the timestamp tick frequency
                        int64_t tickFrequency = rcg::getInteger(nodemap, "GevTimestampTickFrequency");

                        // Calculate the elapsed time in seconds
                        double elapsedTime = static_cast<double>(timestamp) / tickFrequency;

                        std::cout << "Device ID: " << device->getID() << std::endl;
                        std::cout << "Timestamp (ticks): " << timestamp << std::endl;
                        std::cout << "Tick Frequency (Hz): " << tickFrequency << std::endl;
                        std::cout << "Elapsed Time (seconds): " << elapsedTime << std::endl;
                    }
                    catch (const std::exception &ex)
                    {
                        std::cerr << "Failed to retrieve timestamp: " << ex.what() << std::endl;
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
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }

    // Clean up the GenICam systems
    rcg::System::clearSystems();

    return 0;
}


// [https://docs.baslerweb.com/timestamp?utm_source=chatgpt.com]
// PTP disabled: 125 MHz (= 125 000 000 ticks per second, 1 tick = 8 ns)
// PTP enabled: 1 GHz (= 1 000 000 000 ticks per second, 1 tick = 1 ns)
// Timestamp could be set 