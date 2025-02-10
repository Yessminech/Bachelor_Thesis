#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>

using namespace rcg;

int main()
{
    try
    {
        // Get the available GenICam systems.
        auto systems = System::getSystems();
        if (systems.empty())
        {
            throw std::runtime_error("No GenICam systems found.");
        }

        // For this example, we use the first available system.
        systems[0]->open();
        auto interfaces = systems[0]->getInterfaces();
        if (interfaces.empty())
        {
            throw std::runtime_error("No interfaces found.");
        }
        // Open the first interface and get its devices (cameras).
        interfaces[0]->open();
        auto cameras = interfaces[0]->getDevices();
        if (cameras.empty())
        {
            throw std::runtime_error("No camera devices found.");
        }

        // Loop over each camera and configure it for synchronous free run.
        for (size_t i = 0; i < cameras.size(); ++i)
        {
            // Open the camera connection in CONTROL mode.
            cameras[i]->open(Device::CONTROL);

            // Get the remote node map from the camera.
            auto nodemap = cameras[i]->getRemoteNodeMap();
            if (!nodemap)
            {
                std::cerr << "No remote node map available for camera "
                          << cameras[i]->getID() << std::endl;
                cameras[i]->close();
                continue;
            }

            // Set the trigger selector to "FrameStart".
            auto triggerSelector = nodemap->GetNode("TriggerSelector");
            if (triggerSelector)
            {
                triggerSelector->SetValue("FrameStart");
            }
            else
            {
                std::cerr << "TriggerSelector node not found for camera "
                          << cameras[i]->getID() << std::endl;
            }

            // Set the trigger mode to "Off" to enable free run.
            auto triggerMode = nodemap->GetNode("TriggerMode");
            if (triggerMode)
            {
                triggerMode->SetValue("Off");
            }
            else
            {
                std::cerr << "TriggerMode node not found for camera "
                          << cameras[i]->getID() << std::endl;
            }

            // Set the free run start time values to 0.
            auto startTimeLow = nodemap->GetNode("SyncFreeRunTimerStartTimeLow");
            if (startTimeLow)
            {
                startTimeLow->SetValue(0);
            }
            else
            {
                std::cerr << "SyncFreeRunTimerStartTimeLow node not found for camera "
                          << cameras[i]->getID() << std::endl;
            }

            auto startTimeHigh = nodemap->GetNode("SyncFreeRunTimerStartTimeHigh");
            if (startTimeHigh)
            {
                startTimeHigh->SetValue(0);
            }
            else
            {
                std::cerr << "SyncFreeRunTimerStartTimeHigh node not found for camera "
                          << cameras[i]->getID() << std::endl;
            }

            // Set the trigger rate to 30 frames per second.
            auto triggerRate = nodemap->GetNode("SyncFreeRunTimerTriggerRateAbs");
            if (triggerRate)
            {
                triggerRate->SetValue(30.0);
            }
            else
            {
                std::cerr << "SyncFreeRunTimerTriggerRateAbs node not found for camera "
                          << cameras[i]->getID() << std::endl;
            }

            // Apply the changes by executing the update command.
            auto timerUpdate = nodemap->GetNode("SyncFreeRunTimerUpdate");
            if (timerUpdate)
            {
                timerUpdate->Execute();
            }
            else
            {
                std::cerr << "SyncFreeRunTimerUpdate node not found for camera "
                          << cameras[i]->getID() << std::endl;
            }

            // Enable Synchronous Free Run.
            auto timerEnable = nodemap->GetNode("SyncFreeRunTimerEnable");
            if (timerEnable)
            {
                timerEnable->SetValue(true);
            }
            else
            {
                std::cerr << "SyncFreeRunTimerEnable node not found for camera "
                          << cameras[i]->getID() << std::endl;
            }

            // Close the camera connection.
            cameras[i]->close();
        }

        // Clean up: close the interface and system.
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

// void enableSyncFreeRun(Pylon::CBaslerGigEInstantCamera& camera, float frame_rate)
// for (auto& camera : cameras)
// {
//     enableSyncFreeRun(camera, frame_rate);
//     camera.StartGrabbing();
// }


// Retrieve Images Continuously
// while (ros::ok())
// {
//     for (auto& camera : cameras)
//     {
//         camera.RetrieveResult(1, grab_result, Pylon::TimeoutHandling_Return);
//         ros::spinOnce();
//     }
// }

// If a camera is not found within load_camera_timeout, it logs an error and exits.

// Uses `ros::Rate r(0.5);` to retry every 2 seconds if a camera is not found.



// /**
//  * @brief Enable synchronous free‑run mode on a camera.
//  *
//  * This sets the trigger rate, start times to 0 (for simultaneous capture), updates the settings,
//  * and enables the free‑run timer.
//  */
// void enableSyncFreeRun(std::shared_ptr<rcg::Device> camera, float frame_rate)
// {
//     camera->open(rcg::Device::CONTROL);
//     auto nodemap = camera->getRemoteNodeMap();
//     if (nodemap)
//     {
//         auto triggerRateNode = nodemap->GetNode("SyncFreeRunTimerTriggerRateAbs");
//         if (triggerRateNode)
//             triggerRateNode->SetValue(frame_rate);
//         auto startTimeLowNode = nodemap->GetNode("SyncFreeRunTimerStartTimeLow");
//         if (startTimeLowNode)
//             startTimeLowNode->SetValue(0);
//         auto startTimeHighNode = nodemap->GetNode("SyncFreeRunTimerStartTimeHigh");
//         if (startTimeHighNode)
//             startTimeHighNode->SetValue(0);
//         auto updateNode = nodemap->GetNode("SyncFreeRunTimerUpdate");
//         if (updateNode)
//             updateNode->Execute();
//         auto enableNode = nodemap->GetNode("SyncFreeRunTimerEnable");
//         if (enableNode)
//             enableNode->SetValue(true);
//     }
//     camera->close();
// }