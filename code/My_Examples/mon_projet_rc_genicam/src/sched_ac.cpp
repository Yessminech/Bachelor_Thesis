#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/stream.h>
#include <rc_genicam_api/buffer.h>
#include <rc_genicam_api/nodemap_out.h>
#include <GenApi/ICommand.h>

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>

std::shared_ptr<rcg::Stream> stream;

void configureActionCommandDevice(std::shared_ptr<rcg::Device> device, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, const char *triggerSource = "Action1", uint32_t actionSelector = 1)
{
    try
    {
            auto nodemap = device->getRemoteNodeMap();

        rcg::setString(nodemap, "ActionUnconditionalMode", "On");

        // Set Action Device Key, Group Key, and Group Mask
        rcg::setInteger(nodemap, "ActionDeviceKey", actionDeviceKey);
        rcg::setInteger(nodemap, "ActionGroupKey", groupKey);
        rcg::setInteger(nodemap, "ActionGroupMask", groupMask);
        rcg::setInteger(nodemap, "ActionSelector", actionSelector);

        // Configure Triggering
        rcg::setEnum(nodemap, "TriggerSelector", "FrameStart");
        rcg::setEnum(nodemap, "TriggerMode", "On");
        rcg::setEnum(nodemap, "TriggerSource", triggerSource);
            std::cout << "✅ Camera " << device->getID() << " configured to start acquisition on Action Command.\n";
    std::cout << "✅ Action Device Key: " << rcg::getInteger(nodemap, "ActionDeviceKey") << ", Group Key: " << rcg::getInteger(nodemap, "ActionGroupKey") << ", Group Mask: " << rcg::getInteger(nodemap, "ActionGroupMask") << ", ActionSelector: " << rcg::getInteger(nodemap, "ActionSelector") << std::endl;

    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Failed to configure Action Command Device: " << e.what() << std::endl;
    }

}

void configureActionCommandInterface(std::shared_ptr<rcg::Interface> interf, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, std::string triggerSource = "Action1", uint32_t actionSelector = 1, uint32_t destinationIP = 0xFFFFFFFF)
{

    try
    {
        auto nodemap = interf->getNodeMap();
        rcg::setInteger(nodemap, "GevActionDestinationIPAddress", destinationIP);
        // Set Action Device Key, Group Key, and Group Mask
        rcg::setInteger(nodemap, "ActionDeviceKey", actionDeviceKey);
        rcg::setInteger(nodemap, "ActionGroupKey", groupKey);
        rcg::setInteger(nodemap, "ActionGroupMask", groupMask);
        rcg::setBoolean(nodemap, "ActionScheduledTimeEnable", true);
        // ✅ Calculate the execution time (e.g., 5 seconds from now)
        auto now = std::chrono::system_clock::now().time_since_epoch();
        uint64_t currTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        uint64_t executeTime = currTimeNs + 5000000000ULL; // 5 seconds later
        rcg::setInteger(nodemap, "ActionScheduledTime", executeTime);

        std::cout << "✅ Interface configured to start acquisition on Action Command.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Failed to configure Action Command Interface: " << e.what() << std::endl;
    }
}

// ✅ Function to synchronize PTP time before sending Action Command
void syncPTP(std::shared_ptr<rcg::Device> device)
{
    auto nodemap = device->getRemoteNodeMap();
    try
    {
        rcg::callCommand(nodemap, "PtpDataSetLatch"); // Latch PTP timestamp
        uint64_t timestamp = rcg::getInteger(nodemap, "PtpDataSetLatchValue");
        std::cout << "✅ PTP Time synchronized: " << timestamp << " ns\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Failed to sync PTP time: " << e.what() << std::endl;
    }
}

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
        std::cerr << "Failed to set " << feature << ": - Trying deprecated feature: GevIEEE1588" << std::endl;
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


// ✅ Function to send an Action Command
void sendActionCommand(std::shared_ptr<rcg::System> system)
{

    // Fire the Action Command
    try
    {
        auto nodemap = system->getNodeMap();

        rcg::callCommand(nodemap, "ActionCommand");
            std::cout << "✅ Action Command scheduled \n"; // for execution at: " << executeTime << " ns.

    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Failed to send Action Command: " << e.what() << std::endl;
    }

}

bool startStreaming(std::shared_ptr<rcg::Device> device)
{
    try
    {
        std::shared_ptr<GenApi::CNodeMapRef> nodemap = device->getRemoteNodeMap();
        enablePTP(nodemap); // ToDo: Extend to multiple cameras

        auto streams = device->getStreams();
        if (streams.empty())
        {
            std::cerr << "No streams found!" << std::endl;
            return false;
        }

        stream = streams[0];
        stream->open();
        stream->startStreaming();

        std::cout << "\033[1;32mStreaming started. Press Ctrl+C to stop.\033[0m" << std::endl;
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return false;
    }
}

// Function to stop streaming
void stopStreaming(std::shared_ptr<rcg::Device> device)
{
    if (stream)
    {
        stream->stopStreaming();
        stream->close();
        std::cout << "Streaming stopped." << std::endl;
    }

    if (device)
    {
        device->close();
        std::cout << "Device closed." << std::endl;
    }

    rcg::System::clearSystems();
}

int main()
{
    try
    {
        // Define Action Command parameters
        uint32_t actionDeviceKey = 0x12345678;
        uint32_t groupKey = 0x1;
        uint32_t groupMask = 0x1;

        // ✅ Retrieve all available systems
        auto systems = rcg::System::getSystems();

        for (const auto &system : systems)
        {
            system->open();
            auto interfaces = system->getInterfaces();

            for (const auto &interf : interfaces)
            {
                interf->open();
                auto devices = interf->getDevices();

                // ✅ Configure each camera
                for (const auto &device : devices)
                {
                    device->open(rcg::Device::EXCLUSIVE);
                    syncPTP(device); // Sync PTP before configuring Action Commands
                    configureActionCommandDevice(device, actionDeviceKey, groupKey, groupMask, "Action0", 0);
                    configureActionCommandInterface(interf, actionDeviceKey, groupKey, groupMask, "Action0", 0, 0xFFFFFFFF);
                    // ✅ Send the Action Command
                    sendActionCommand(system);
                    device->close();
                }

                // ✅ Acquire Image after Action Command
                for (const auto &device : devices)
                {
                    device->open(rcg::Device::CONTROL);
                    std::cout << "✅ Number of queued Actions: " << rcg::getInteger(device->getRemoteNodeMap(), "ActionQueueSize") << " \n";
                    ;
                    auto streams = device->getStreams();
                    if (streams.empty())
                    {
                        std::cerr << "⚠️ No streams found for device " << device->getID() << std::endl;
                        return 1;
                    }

                    stream = streams[0];
                    stream->open();
                    stream->startStreaming();

                    // ✅ Wait for an image
                    const rcg::Buffer *buffer = stream->grab(8000); // 5-second timeout

                    if (buffer && buffer->getTimestamp() > 0)
                    {
                        std::cout << "✅ Image acquired at timestamp: " << buffer->getTimestamp() << " ns\n";
                    }
                    else
                    {
                        std::cerr << "⚠️ No image received! Action Command may have failed.\n";
                    }
        stopStreaming(device);

                    // ✅ Cleanup
                    stream->close();
                    device->close();
                }

                interf->close();
            }

            system->close();
        }

        // ✅ Clean up the GenICam systems
        rcg::System::clearSystems();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "❌ Exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
