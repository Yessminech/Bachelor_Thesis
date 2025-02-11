// https://github.com/iron-ox/basler_camera/blob/iron-kinetic-devel/src/basler_synced_cameras_node.cpp#L210

#include <ros/ros.h>
#include <ros/console.h>
#include <XmlRpcValue.h>

#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>
#include <rc_genicam_api/exception.h>

// ROS camera info includes
#include <camera_info_manager/camera_info_manager.h>
#include <sensor_msgs/CameraInfo.h>

// Your (re‑implemented) parameter and image handling classes for GenICam cameras.
#include "basler_parameters.h"   // adjust name if needed
#include "image_publisher.h"      // adjust name if needed

#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace {
  const int load_camera_timeout = 5;   // seconds
  const int ptp_sync_timeout    = 60;   // seconds
}

/**
 * @brief Load cameras by serial numbers from YAML using the rc_genicam API.
 *
 * @param cameras_yaml The YAML array describing each camera.
 * @return std::vector<std::shared_ptr<rcg::Device>> A vector of found cameras.
 */
std::vector<std::shared_ptr<rcg::Device>> loadCameras(const XmlRpc::XmlRpcValue& cameras_yaml)
{
    // Get available systems and open the first one.
    auto systems = rcg::System::getSystems();
    if (systems.empty()) {
        ROS_ERROR("No GenICam systems found.");
        return {};
    }
    systems[0]->open();
   
    // Open the first interface on that system.
    auto interfaces = systems[0]->getInterfaces();
    if (interfaces.empty()) {
        ROS_ERROR("No interfaces found.");
        return {};
    }
    interfaces[0]->open();

    std::vector<std::shared_ptr<rcg::Device>> cameras(cameras_yaml.size());
    ros::Rate r(0.5);

    for (size_t index = 0; index < cameras_yaml.size(); ++index)
    {
        ROS_ASSERT_MSG(cameras_yaml[index].getType() == XmlRpc::XmlRpcValue::TypeStruct,
                       "Bad YAML for camera %zu", index);
        ROS_ASSERT_MSG(cameras_yaml[index].hasMember("serial"),
                       "Camera %zu YAML missing serial", index);
        // In this example the serial is stored as an integer.
        std::string serial = std::to_string(int(cameras_yaml[index]["serial"]));

        bool camera_found = false;
        ros::Time timeout_time = ros::Time::now() + ros::Duration(load_camera_timeout);
        while (ros::ok() && !camera_found)
        {
            auto devices = interfaces[0]->getDevices();
            for (size_t i = 0; i < devices.size(); ++i)
            {
                // Assume your rc_genicam Device class provides getSerial().
                if (devices[i]->getSerial() == serial)
                {
                    ROS_INFO_STREAM("Found camera " << index << " with matching serial " << serial);
                    cameras[index] = devices[i];
                    camera_found = true;
                    break;
                }
            }
            if (!camera_found)
            {
                ROS_DEBUG_STREAM("Failed to find camera " << index << " with serial " << serial);
                ROS_DEBUG_STREAM(interfaces[0]->getDevices().size() << " other devices found.");
                if (ros::Time::now() > timeout_time)
                {
                    ROS_ERROR_STREAM("Timed out looking for camera " << index << " with serial " << serial);
                    return {};
                }
                r.sleep();
                ros::spinOnce();
            }
        }
    }
    return cameras;
}

/**
 * @brief Wait until all cameras report they are PTP slaves and share the same master clock.
 *
 * This function polls each camera’s remote nodemap for PTP status.
 */
bool waitForPTPSlave(const std::vector<std::shared_ptr<rcg::Device>>& cameras)
{
    ROS_DEBUG("Waiting for %zu cameras to be PTP slaves...", cameras.size());
    ros::Rate r(0.5);
    ros::Time timeout_time = ros::Time::now() + ros::Duration(ptp_sync_timeout);

    while (ros::ok())
    {
        int num_init   = 0;
        int num_master = 0;
        int num_slave  = 0;
        int64_t master_clock_id = 0;
        bool multiple_masters = false;

        for (const auto& camera : cameras)
        {
            camera->open(rcg::Device::CONTROL);
            auto nodemap = camera->getRemoteNodeMap();
            if (!nodemap)
            {
                camera->close();
                continue;
            }
            // Latch the PTP status.
            auto latchNode = nodemap->GetNode("GevIEEE1588DataSetLatch");
            if (latchNode)
                latchNode->Execute();
            // Read the latched PTP status.
            auto statusNode = nodemap->GetNode("GevIEEE1588StatusLatched");
            int status = (statusNode) ? statusNode->GetValue() : 0;
            // Assume: 1 = Master, 2 = Slave; other values indicate initialization.
            if (status == 1) {
                ++num_master;
            } else if (status == 2) {
                auto parentNode = nodemap->GetNode("GevIEEE1588ParentClockId");
                int64_t master_id = (parentNode) ? parentNode->GetValue() : 0;
                if (num_slave == 0)
                    master_clock_id = master_id;
                else if (master_id != master_clock_id)
                    multiple_masters = true;
                ++num_slave;
            } else {
                ++num_init;
            }
            camera->close();
        }
        if (num_slave == cameras.size() && !multiple_masters)
        {
            ROS_DEBUG_STREAM("All camera clocks are PTP slaves to master clock " << master_clock_id);
            return true;
        }
        else if (num_slave == cameras.size())
        {
            ROS_DEBUG("All camera clocks are PTP slaves, but multiple masters are present");
        }
        else
        {
            ROS_DEBUG("Camera PTP status: %d initializing, %d masters, %d slaves", num_init, num_master, num_slave);
        }
        if (ros::Time::now() > timeout_time)
        {
            ROS_ERROR("Timed out waiting for camera clocks to become PTP slaves. Status: %d initializing, %d masters, %d slaves",
                      num_init, num_master, num_slave);
            return false;
        }
        r.sleep();
        ros::spinOnce();
    }
    return false;
}

/**
 * @brief Wait until all camera clocks are synchronized to the master clock.
 *
 * Synchronization is declared when all cameras have an offset from master below max_offset_ns
 * for at least offset_window_s seconds.
 */
bool waitForPTPClockSync(const std::vector<std::shared_ptr<rcg::Device>>& cameras, int max_offset_ns, int offset_window_s)
{
    ROS_DEBUG("Waiting for clock offsets < %d ns over %d s...", max_offset_ns, offset_window_s);
    bool currently_below = false;
    ros::Time below_start;
    std::vector<int64_t> current_offsets(cameras.size(), 0);
    int64_t current_max_abs_offset = 0;

    ros::Time timeout_time = ros::Time::now() + ros::Duration(ptp_sync_timeout);
    while (ros::ok())
    {
        current_max_abs_offset = 0;
        for (size_t i = 0; i < cameras.size(); ++i)
        {
            cameras[i]->open(rcg::Device::CONTROL);
            auto nodemap = cameras[i]->getRemoteNodeMap();
            if (nodemap)
            {
                auto latchNode = nodemap->GetNode("GevIEEE1588DataSetLatch");
                if (latchNode) latchNode->Execute();
                auto offsetNode = nodemap->GetNode("GevIEEE1588OffsetFromMaster");
                if (offsetNode)
                {
                    current_offsets[i] = offsetNode->GetValue();
                    current_max_abs_offset = std::max(current_max_abs_offset,
                                                      static_cast<int64_t>(std::abs(current_offsets[i])));
                }
            }
            cameras[i]->close();
        }
        if (current_max_abs_offset < max_offset_ns)
        {
            if (!currently_below)
            {
                currently_below = true;
                below_start = ros::Time::now();
            }
            else if ((ros::Time::now() - below_start).toSec() >= offset_window_s)
            {
                ROS_DEBUG("All clocks synced");
                return true;
            }
        }
        else
        {
            currently_below = false;
        }
        if (ros::Time::now() > timeout_time)
        {
            ROS_ERROR("PTP clock synchronization timed out waiting for offsets < %d ns over %d s", max_offset_ns, offset_window_s);
            ROS_ERROR("Current clock offsets:");
            for (size_t i = 0; i < cameras.size(); ++i)
            {
                ROS_ERROR_STREAM(i << ": " << current_offsets[i]);
            }
            return false;
        }
        ros::spinOnce();
        ros::Duration(0.5).sleep();
    }
    return false;
}

/**
 * @brief Enable synchronous free‑run mode on a camera.
 *
 * This sets the trigger rate, start times to 0 (for simultaneous capture), updates the settings,
 * and enables the free‑run timer.
 */
void enableSyncFreeRun(std::shared_ptr<rcg::Device> camera, float frame_rate)
{
    camera->open(rcg::Device::CONTROL);
    auto nodemap = camera->getRemoteNodeMap();
    if (nodemap)
    {
        auto triggerRateNode = nodemap->GetNode("SyncFreeRunTimerTriggerRateAbs");
        if (triggerRateNode)
            triggerRateNode->SetValue(frame_rate);
        auto startTimeLowNode = nodemap->GetNode("SyncFreeRunTimerStartTimeLow");
        if (startTimeLowNode)
            startTimeLowNode->SetValue(0);
        auto startTimeHighNode = nodemap->GetNode("SyncFreeRunTimerStartTimeHigh");
        if (startTimeHighNode)
            startTimeHighNode->SetValue(0);
        auto updateNode = nodemap->GetNode("SyncFreeRunTimerUpdate");
        if (updateNode)
            updateNode->Execute();
        auto enableNode = nodemap->GetNode("SyncFreeRunTimerEnable");
        if (enableNode)
            enableNode->SetValue(true);
    }
    camera->close();
}

int main(int argc, char* argv[])
{
    ros::init(argc, argv, "genicam_synced_cameras");
    ros::NodeHandle nh("");
    ros::NodeHandle pnh("~");

    // Load launch file parameters.
    int ptp_max_offset_ns;
    pnh.param("ptp_max_offset_ns", ptp_max_offset_ns, 1000);
    int ptp_offset_window_s;
    pnh.param("ptp_window_s", ptp_offset_window_s, 5);
    float frame_rate;
    pnh.param("frame_rate", frame_rate, 5.f);

    // Load camera and GenICam parameters from YAML.
    XmlRpc::XmlRpcValue cameras_yaml;
    pnh.getParam("cameras", cameras_yaml);
    ROS_ASSERT_MSG(cameras_yaml.getType() == XmlRpc::XmlRpcValue::TypeArray, "Bad cameras YAML");
    XmlRpc::XmlRpcValue genicam_params;
    pnh.getParam("genicam_params", genicam_params);
    ROS_ASSERT_MSG(genicam_params.getType() == XmlRpc::XmlRpcValue::TypeArray, "Bad genicam_params YAML");

    // Load cameras by serial.
    auto cameras = loadCameras(cameras_yaml);
    if (cameras.empty())
        return 1;

    std::vector<std::shared_ptr<camera_info_manager::CameraInfoManager>> cinfo_managers(cameras.size());
    // Set up camera infos and register image event handlers.
    for (size_t i = 0; i < cameras.size(); ++i)
    {
        ROS_ASSERT_MSG(cameras_yaml[i].hasMember("name"), "Camera %zu YAML missing name", i);
        ROS_ASSERT_MSG(cameras_yaml[i].hasMember("camera_info_url"), "Camera %zu YAML missing camera_info_url", i);
        ROS_ASSERT_MSG(cameras_yaml[i].hasMember("frame_id"), "Camera %zu YAML missing frame_id", i);
        std::string name = static_cast<std::string>(cameras_yaml[i]["name"]);
        std::string camera_info_url = static_cast<std::string>(cameras_yaml[i]["camera_info_url"]);
        std::string frame_id = static_cast<std::string>(cameras_yaml[i]["frame_id"]);

        std::string camera_info_dir;
        if (pnh.getParam("camera_info_dir", camera_info_dir))
            camera_info_url = camera_info_dir + "/" + camera_info_url;

        ros::NodeHandle camera_nh(nh, name);
        cinfo_managers[i] = std::make_shared<camera_info_manager::CameraInfoManager>(camera_nh, name);
        cinfo_managers[i]->loadCameraInfo(camera_info_url);

        sensor_msgs::CameraInfo::Ptr cinfo(new sensor_msgs::CameraInfo(cinfo_managers[i]->getCameraInfo()));

        // Register image event handler.
        // (Replace ImagePublisher with your actual implementation for genicam_rc.)
        cameras[i]->RegisterImageEventHandler(new ImagePublisher(nh, cinfo, frame_id, name + "/", true),
                                                rcg::RegistrationMode_Append,
                                                rcg::Cleanup_Delete);
        // Register continuous acquisition configuration.
        cameras[i]->RegisterConfiguration(new AcquireContinuousConfiguration,
                                           rcg::RegistrationMode_ReplaceAll,
                                           rcg::Cleanup_Delete);
    }

    // Open cameras to reset parameters, enable PTP, and load camera-specific parameters.
    for (auto& camera : cameras)
    {
        camera->open(rcg::Device::CONTROL);
        auto nodemap = camera->getRemoteNodeMap();
        if (nodemap)
        {
            // Reset to default user set.
            auto userSetSelector = nodemap->GetNode("UserSetSelector");
            if (userSetSelector)
                userSetSelector->SetValue("Default");
            auto userSetLoad = nodemap->GetNode("UserSetLoad");
            if (userSetLoad)
                userSetLoad->Execute();
            // Enable PTP.
            auto ptpNode = nodemap->GetNode("GevIEEE1588");
            if (ptpNode)
                ptpNode->SetValue(true);
        }
        // Handle additional camera parameters.
        handle_basler_parameters(camera);
        camera->close();
    }

    ROS_INFO("Waiting for camera PTP clock synchronization...");
    if (!waitForPTPSlave(cameras) ||
        !waitForPTPClockSync(cameras, ptp_max_offset_ns, ptp_offset_window_s))
    {
        return 1;
    }

    ROS_INFO("All %zu cameras synced. Starting image capture.", cameras.size());
    for (auto& camera : cameras)
    {
        enableSyncFreeRun(camera, frame_rate);
        camera->startGrabbing();  // Assumes rc_genicam API provides startGrabbing()
    }

    // Main grabbing loop.
    // (Assuming rc_genicam API provides a GrabResult class and retrieveResult() method.)
    std::shared_ptr<rcg::GrabResult> grab_result;
    while (ros::ok())
    {
        for (auto& camera : cameras)
        {
            camera->retrieveResult(1, grab_result, rcg::TimeoutHandling_Return);
            ros::spinOnce();
        }
    }

    return 0;
}

    // // Load in launchfile params
    // int ptp_max_offset_ns;
    // pnh.param("ptp_max_offset_ns", ptp_max_offset_ns, 1000);
    // int ptp_offset_window_s;
    // pnh.param("ptp_window_s", ptp_offset_window_s, 5);
    // float frame_rate;
    // pnh.param("frame_rate", frame_rate, 5.f);
    // // We enable PTP before setting other camera parameters, since the GevSCPD parameter in particular has different units depending on if PTP is on or off.

    // ROS_INFO("Waiting for camera PTP clock synchronization...");
    // if (!waitForPTPSlave(cameras) ||
    //     !waitForPTPClockSync(cameras, ptp_max_offset_ns, ptp_offset_window_s))
    // {
    //     return 1;
    // }
    //     ROS_INFO("All %zu cameras synced. Starting image capture.", cameras.size());
    // for (auto& camera : cameras)
    // {
    //     enableSyncFreeRun(camera, frame_rate);
    //     camera.StartGrabbing();
    // }

    // Pylon::CGrabResultPtr grab_result;
    // while (ros::ok())
    // {
    //     for (auto& camera : cameras)
    //     {
    //         camera.RetrieveResult(1, grab_result, Pylon::TimeoutHandling_Return);
    //         ros::spinOnce();
    //     }
    // }

