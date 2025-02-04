/***** GPT Example *****/

#include <rc_genicam_api/system.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

// Helper function to print usage information
void printHelp()
{
    std::cout << "Usage: print_features <device-id>" << std::endl;
    std::cout << "  <device-id>  GenICam device ID, serial number, or user-defined device name." << std::endl;
    std::cout << std::endl;
    std::cout << "This application connects to the specified device and prints all available GenICam features." << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printHelp();
        return 1;
    }

    try
    {
        // Retrieve the device ID from the command-line arguments.
        std::string deviceID = argv[1];

        // Find the device by its GenICam identifier.
        std::shared_ptr<rcg::Device> device = rcg::getDevice(deviceID.c_str());
        if (!device)
        {
            std::cerr << "Device '" << deviceID << "' not found!" << std::endl;
            return 1;
        }

        // Open the device in CONTROL mode to access its remote node map.
        device->open(rcg::Device::CONTROL);
        std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();

        std::cout << "Printing all GenICam features for device '" << deviceID << "':" << std::endl << std::endl;

        // The rc_genicam_api function printNodemap prints all nodes from the provided nodemap.
        // The first argument is the nodemap, the second is an optional filter (empty here to print all nodes),
        // the third is the maximum depth (set to 100), and the fourth indicates whether to indent the output.
        if (!rcg::printNodemap(nodemap, "", 100, false))
        {
            std::cerr << "Failed to print the nodemap features." << std::endl;
        }

        // Close the device after finishing.
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

    // Clean up and release all GenICam systems.
    rcg::System::clearSystems();

    return 0;
}

