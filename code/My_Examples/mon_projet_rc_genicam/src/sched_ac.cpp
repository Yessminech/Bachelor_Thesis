#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>
#include <rc_genicam_api/exception.h>

#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cstdint>
#include <string>

using namespace rcg;

int main()
{
    try
    {
        // Retrieve the available GenICam systems.
        auto systems = System::getSystems();
        if (systems.empty())
        {
            throw std::runtime_error("No GenICam systems found.");
        }
        // Open the first system.
        systems[0]->open();

        // Retrieve the interfaces from the system.
        auto interfaces = systems[0]->getInterfaces();
        if (interfaces.empty())
        {
            throw std::runtime_error("No interfaces found.");
        }
        // Open the first interface.
        interfaces[0]->open();

        // Retrieve the camera devices from the interface.
        auto cameras = interfaces[0]->getDevices();
        if (cameras.empty())
        {
            throw std::runtime_error("No camera devices found.");
        }

        //--- Start of camera setup ---
        // Configure each camera for synchronous image acquisition.
        for (size_t i = 0; i < cameras.size(); ++i)
        {
            // Open the camera connection in CONTROL mode to access its remote nodemap.
            cameras[i]->open(Device::CONTROL);

            // Retrieve the remote node map.
            auto nodemap = cameras[i]->getRemoteNodeMap();
            if (!nodemap)
            {
                std::cerr << "No remote node map available for camera "
                          << cameras[i]->getID() << std::endl;
                cameras[i]->close();
                continue;
            }

            // Configure the trigger selector: select "FrameStart".
            {
                auto triggerSelector = nodemap->GetNode("TriggerSelector");
                if (triggerSelector)
                {
                    triggerSelector->SetValue("FrameStart");
                }
                else
                {
                    std::cerr << "TriggerSelector not found for camera "
                              << cameras[i]->getID() << std::endl;
                }
            }

            // Configure the trigger mode: set it to "On".
            {
                auto triggerMode = nodemap->GetNode("TriggerMode");
                if (triggerMode)
                {
                    triggerMode->SetValue("On");
                }
                else
                {
                    std::cerr << "TriggerMode not found for camera "
                              << cameras[i]->getID() << std::endl;
                }
            }

            // Configure the trigger source: select "Action1".
            {
                auto triggerSource = nodemap->GetNode("TriggerSource");
                if (triggerSource)
                {
                    triggerSource->SetValue("Action1");
                }
                else
                {
                    std::cerr << "TriggerSource not found for camera "
                              << cameras[i]->getID() << std::endl;
                }
            }

            // Specify the action device key (e.g., 4711).
            {
                auto actionDeviceKey = nodemap->GetNode("ActionDeviceKey");
                if (actionDeviceKey)
                {
                    actionDeviceKey->SetValue(4711);
                }
                else
                {
                    std::cerr << "ActionDeviceKey not found for camera "
                              << cameras[i]->getID() << std::endl;
                }
            }

            // Set the action group key (here, all cameras are in group 1).
            {
                auto actionGroupKey = nodemap->GetNode("ActionGroupKey");
                if (actionGroupKey)
                {
                    actionGroupKey->SetValue(1);
                }
                else
                {
                    std::cerr << "ActionGroupKey not found for camera "
                              << cameras[i]->getID() << std::endl;
                }
            }

            // Set the action group mask (all cameras will respond to any mask != 0).
            {
                auto actionGroupMask = nodemap->GetNode("ActionGroupMask");
                if (actionGroupMask)
                {
                    actionGroupMask->SetValue(0xffffffff);
                }
                else
                {
                    std::cerr << "ActionGroupMask not found for camera "
                              << cameras[i]->getID() << std::endl;
                }
            }

            // Close the camera after configuration.
            cameras[i]->close();
        }
        //--- End of camera setup ---

        // Get the current timestamp of the first camera.
        // NOTE: All cameras must be synchronized via Precision Time Protocol (PTP).
        // Reopen the first camera to access its remote nodemap.
        cameras[0]->open(Device::CONTROL);
        auto nodemap = cameras[0]->getRemoteNodeMap();
        if (!nodemap)
        {
            throw std::runtime_error("Failed to retrieve remote node map from first camera.");
        }

        // Latch the current timestamp.
        {
            auto timestampLatch = nodemap->GetNode("GevTimestampControlLatch");
            if (timestampLatch)
            {
                timestampLatch->Execute();
            }
            else
            {
                throw std::runtime_error("GevTimestampControlLatch node not found.");
            }
        }

        // Retrieve the latched timestamp value.
        int64_t currentTimestamp = 0;
        {
            auto timestampNode = nodemap->GetNode("GevTimestampValue");
            if (timestampNode)
            {
                currentTimestamp = timestampNode->GetValue();
                std::cout << "Current timestamp: " << currentTimestamp << std::endl;
            }
            else
            {
                throw std::runtime_error("GevTimestampValue node not found.");
            }
        }
        cameras[0]->close();

        // Specify that the scheduled action should be executed roughly 30 seconds
        // (30,000,000,000 ticks) after the current timestamp.
        int64_t actionTime = currentTimestamp + 30000000000LL;

        // Issue a scheduled action command to the cameras.
        // This command is normally issued via the GenTL layer.
        // For example, if rc_genicam exposes the GenTL wrapper, you might do:
        /*
        auto genTL = systems[0]->getGenTLWrapper();
        if (genTL)
        {
            genTL->IssueScheduledActionCommand(4711, 1, 0xffffffff, actionTime, "192.168.1.255");
        }
        else
        {
            std::cerr << "GenTL wrapper not available to issue scheduled action command." << std::endl;
        }
        */
        // For demonstration, we simply print the scheduled action command parameters.
        std::cout << "Scheduled Action Command:" << std::endl;
        std::cout << "  Action Device Key: 4711" << std::endl;
        std::cout << "  Action Group Key: 1" << std::endl;
        std::cout << "  Action Group Mask: 0xffffffff" << std::endl;
        std::cout << "  Action Time: " << actionTime << std::endl;
        std::cout << "  Destination IP: 192.168.1.255" << std::endl;

        // Cleanup: close the interface and system.
        interfaces[0]->close();
        systems[0]->close();
        System::clearSystems();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
