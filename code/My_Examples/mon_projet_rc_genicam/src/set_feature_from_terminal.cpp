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
    std::cout << "  genicam_example [OPTION?] command <parameters>\n\n";
    std::cout << "Small utility for basic control of a Genicam device.\n\n";
    std::cout << "Help Options:\n";
    std::cout << "  -h, --help                                        Show help options\n\n";
    std::cout << "Application Options:\n";
    std::cout << "  -n, --name=<pattern>                              Device name pattern\n";
    std::cout << "  -a, --address=<device_address>                    Device address\n";
    std::cout << "  --register-cache={disable|enable|debug}           Register cache policy\n";
    std::cout << "  --range-check={disable|enable|debug}              Range check policy\n";
    std::cout << "  --access-check={disable|enable}                   Feature access check policy\n";
    std::cout << "  --gv-allow-broadcast-discovery-ack                Allow broadcast discovery ack\n";
    std::cout << "  -t, --time                                        Show execution time\n";
    std::cout << "  -d, --debug={<category>[:<level>][,...]|help}     Debug options\n";
    std::cout << "  -v, --version                                     Show version\n\n";
    std::cout << "Command may be one of the following possibilities:\n\n";
    std::cout << "  genicam:                          dump the content of the Genicam xml data\n";
    std::cout << "  features:                         list all features\n";
    std::cout << "  values:                           list all available feature values\n";
    std::cout << "  description [<feature>] ...:      show the full feature description\n";
    std::cout << "  control <feature>[=<value>] ...:  read/write device features\n";
    std::cout << "  network <setting>[=<value>]...:   read/write network settings\n\n";
    std::cout << "If no command is given, this utility will list all the available devices.\n";
    std::cout << "For the control command, direct access to device registers is provided using a R[address] syntax in place of a feature name.\n\n";
    std::cout << "Examples:\n\n";
    std::cout << "genicam_example control Width=128 Height=128 Gain R[0x10000]=0x10\n";
    std::cout << "genicam_example features\n";
    std::cout << "genicam_example description Width Height\n";
    std::cout << "genicam_example network mode=PersistentIP\n";
    std::cout << "genicam_example network ip=192.168.0.1 mask=255.255.255.0 gateway=192.168.0.254\n";
    std::cout << "genicam_example -n Basler-210ab4 genicam\n";
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
        rcg::setString(nodemap, feature.c_str(), value.c_str(), true);

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
        printHelp();
        return 1;
    }

    std::string command(argv[1]);

    if (command == "-h" || command == "--help")
    {
        printHelp();
        return 0;
    }
    else if (command == "features")
    {
        //listDevices(); // ToDo import
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