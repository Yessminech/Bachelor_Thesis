#include <rc_genicam_api/system.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>
#include <cstring>

// Helper function to print usage instructions.
void printHelp()
{
    std::cout << "Usage:\n";
    std::cout << "  cam_ctrl [OPTION?] command <parameters>\n\n";
    std::cout << "Basic control of a Genicam device.\n\n";
    std::cout << "Help Options:\n";
    std::cout << "  -h, --help                                        Show help options\n\n";
    std::cout << "Application Options:\n";
    std::cout << "  -i, --id=<device_id>                              Device name pattern\n";
    std::cout << "  -a, --address=<device_ip>                         Device address\n";
    std::cout << "Command may be one of the following possibilities:\n\n";
    std::cout << "  features:                         list all features\n";
    std::cout << "  values:                           list all available feature values\n";
    std::cout << "  control <feature>[=<value>] ...:  read/write device features\n";
    std::cout << "If no command is given, this utility will list all the available devices.\n";
    std::cout << "Examples:\n\n";
    std::cout << "cam_ctrl control Width=128 Height=128 Gain R[0x10000]=0x10\n";
    std::cout << "cam_ctrl features\n";
    std::cout << "cam_ctrl -n Basler-210ab4 features\n";
}

void listFeatures(const std::string &deviceID)
{

}

void listValues(const std::string &deviceID)
{

}



// Function to control device features
void controlFeature(const std::string &deviceID, const std::string &feature, const std::string &value)
{
    try
    {
        std::shared_ptr<rcg::Device> device = rcg::getDevice(deviceID.c_str());
        if (!device)
        {
            std::cerr << "Device '" << deviceID << "' not found!" << std::endl;
            return;
        }

        device->open(rcg::Device::CONTROL);
        std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();

        std::cout << "Setting feature '" << feature << "' to '" << value << "'." << std::endl;
        rcg::setString(nodemap, feature.c_str(), value.c_str(), true); // ToDo will this work with all types?
        std::string newValue = rcg::getString(nodemap, feature.c_str(), true);
        std::cout << "Feature '" << feature << "' is now set to: " << newValue << std::endl;

        device->close();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown exception occurred." << std::endl;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "No command given. Listing all available devices. For more information, use -h or --help." << std::endl;
        // listDevices();
        return 0;
    }

    std::string command(argv[1]);

    if (command == "-h" || command == "--help")
    {
        printHelp();
        return 0;
    }
    else if (command == "features")
    {
        listFeatures("");
        return 0;
    }
    else if (command == "values")
    {
        listValues("");
        return 0;
    }
    else if (command == "control" && argc == 5)
    {
        std::string deviceID(argv[2]);
        std::string feature(argv[3]);
        std::string value(argv[4]);
        controlFeature(deviceID, feature, value);
        return 0;
    }
    else
    {
        printHelp();
        return 1;
    }
}