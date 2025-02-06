#include <rc_genicam_api/system.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>

// Helper function to print usage instructions.
void printHelp()
{
    std::cout << "Usage: ./genicam_example <device-id> <feature> <value>" << std::endl;
    std::cout << "Example: ./genicam_example 23630918 AcquisitionMode Continuous" << std::endl;
    std::cout << std::endl;
    std::cout << "This application connects to the specified GenICam device and sets the given feature" << std::endl;
    std::cout << "to the provided value using the remote nodemap." << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printHelp();
        return 1;
    }

    // Parse command-line arguments.
    std::string deviceID(argv[1]);
    std::string feature(argv[2]);
    std::string value(argv[3]);

    try
    {
        // Retrieve the device based on the provided device ID.
        std::shared_ptr<rcg::Device> device = rcg::getDevice(deviceID.c_str());
        if (!device)
        {
            std::cerr << "Device '" << deviceID << "' not found!" << std::endl;
            return 1;
        }

        // Open the device in CONTROL mode to access its remote nodemap.
        device->open(rcg::Device::CONTROL);
        std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();

        // Set the feature using rcg::setString. If the node is of a different type,
        // consider using rcg::setBoolean, rcg::setInteger, or rcg::setFloat as appropriate.
        std::cout << "Setting feature '" << feature << "' to '" << value << "'." << std::endl;
        rcg::setString(nodemap, feature.c_str(), value.c_str(), true);

        // Optionally, read back the feature's value to confirm the change.
        std::string newValue = rcg::getString(nodemap, feature.c_str(), true);
        std::cout << "Feature '" << feature << "' is now set to: " << newValue << std::endl;

        // Close the device.
        device->close();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 2;
    }
    catch (...)
    {
        std::cerr << "Unknown exception occurred." << std::endl;
        return 3;
    }

    // Clean up the GenICam systems.
    rcg::System::clearSystems();

    return 0;
}