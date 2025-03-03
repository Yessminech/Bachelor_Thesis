DeviceConfig getConfig(const std::shared_ptr<rcg::Device> &device)
{ // ToDo: Exceptions handling
  DeviceConfig config;
  config.id = device->getID();
  config.vendor = device->getVendor();
  config.model = device->getModel();
  config.tlType = device->getTLType();
  config.displayName = device->getDisplayName();
  config.accessStatus = device->getAccessStatus();
  config.serialNumber = device->getSerialNumber();
  config.version = device->getVersion();
  config.timestampFrequency = device->getTimestampFrequency();
  config.currentIP = getCurrentIP(device);
  config.MAC = getMAC(device);
  return config;
}

bool getGenTLVersion(std::shared_ptr<GenApi::CNodeMapRef> nodemap)
{
    try
    {
        bool deprecatedFeatures = rcg::getString(nodemap, "PtpEnable").empty();
        if (debug)
        {
            std::cout << GREEN << "Version Check success" << RESET << std::endl;
        }
        return deprecatedFeatures;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get GenTL version: " << ex.what() << RESET << std::endl;
        return false;
    }
}

void enableChunkData(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID)
{
    if (nodemap)
    {
        try
        {
            rcg::setBoolean(nodemap, "ChunkModeActive", true);
            rcg::setBoolean(nodemap, "ChunkDataControl", true); // add deprecated check
            rcg::setBoolean(nodemap, "ChunkEnable", true);      // add deprecated check

            if (debug)
                std::cout << "[DEBUG] Camera " << camID << ": Chunk mode enabled" << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Failed to enable chunk mode for camera " << camID << ": " << ex.what() << RESET << std::endl;
        }
    }
}

void configureSyncFreeRun(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID)
{
    if (nodemap)
    {
        try
        {
            rcg::setEnum(nodemap, "AcquisitionMode", "Continuous");
            rcg::setEnum(nodemap, "TriggerSelector", "FrameStart");
            rcg::setEnum(nodemap, "TriggerMode", "Off");
            rcg::setBoolean(nodemap, "AcquisitionFrameRateEnable", true);
            // rcg::setEnum(nodemap, "AcquisitionFrameRateAuto", "Off");
            double maxFrameRate = 10; // rcg::getFloat(nodemap, "AcquisitionFrameRate");
            try
            {
                rcg::setFloat(nodemap, "AcquisitionFrameRate", maxFrameRate);
            }
            catch (const std::exception &e)
            {
                rcg::setFloat(nodemap, "AcquisitionFrameRateAbs", maxFrameRate);
            }

            if (debug)
                std::cout << "[DEBUG] Camera " << camID << ": Configured free-run mode" << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Failed to configure free-run mode for camera " << camID << ": " << ex.what() << RESET << std::endl;
        }
    }
}

void setupCameraSettings(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID)
{
    if (nodemap)
    {
        try
        {
            // Set exposure time.
            rcg::setFloat(nodemap, "ExposureTimeAbs", 222063); // Example: 20 ms
            rcg::setFloat(nodemap, "ExposureTime", 222063);    // Example: 20 ms

            // rcg::setEnum(nodemap, "ExposureAuto","Continuous");

            if (debug)
                std::cout << "[DEBUG] Camera " << camID << ": Exposure time set to 80 ms" << std::endl;

            // Set gain.
            rcg::setFloat(nodemap, "Gain", 0); // Example: Gain of 10 dB
            if (debug)
                std::cout << "[DEBUG] Camera " << camID << ": Gain set to 20 dB" << std::endl;

            // Set white balance.
            rcg::setBoolean(nodemap, "AutoFunctionROIUseWhiteBalance", true);

            rcg::setEnum(nodemap, "BalanceWhiteAuto", "Continuous"); // Example: Auto white balance
            if (debug)
                std::cout << "[DEBUG] Camera " << camID << ": White balance set to auto" << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Failed to set camera settings for camera " << camID << ": " << ex.what() << RESET << std::endl;
        }
    }
}

/// This thread function grabs images from one camera, converts and resizes them,
/// overlays the device ID, timestamp, and FPS, and updates the shared global frame vector at position 'index'.
/// Once the stream is successfully started, it increments the global atomic counter.
void displayCameraStream(const std::shared_ptr<rcg::Device> &device, int index)
{
    try
    {
        if (debug)
            std::cout << "[DEBUG] Opening device " << device->getID() << std::endl;
        device->open(rcg::Device::CONTROL);
        auto nodemap = device->getRemoteNodeMap();
        configureSyncFreeRun(nodemap, device->getID());
        enableChunkData(nodemap, device->getID());
        setBandwidth(device, index);
        setupCameraSettings(nodemap, device->getID());
        // Get the first available stream.
        std::vector<std::shared_ptr<rcg::Stream>> streamList = device->getStreams();
        if (streamList.empty())
        {
            std::cerr << RED << "No stream available for camera " << device->getID() << RESET << std::endl;
            device->close();
            return;
        }
        auto stream = streamList[0];
        try
        {
            stream->open();
            stream->attachBuffers(true);
            stream->startStreaming();
            if (debug)
                std::cout << "[DEBUG] Stream started for camera " << device->getID() << std::endl;
            // Signal that this thread has started successfully.
            startedThreads++;
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Failed to start stream for camera " << device->getID()
                      << ": " << ex.what() << RESET << std::endl;

            return;
        }

        // Variables for FPS calculation.
        auto lastTime = std::chrono::steady_clock::now();
        int frameCount = 0;

        while (!stop_streaming)
        {
            const rcg::Buffer *buffer = nullptr;
            try
            {
                buffer = stream->grab(5000);
            }
            catch (const std::exception &ex)
            {
                std::cerr << RED << "[DEBUG] Exception during buffer grab for camera "
                          << device->getID() << ": " << ex.what() << RESET << std::endl;
                continue;
            }
            if (buffer && !buffer->getIsIncomplete() && buffer->getImagePresent(0))
            {
                rcg::Image image(buffer, 0);
                cv::Mat outputFrame;
                uint64_t format = image.getPixelFormat();
                cv::Mat rawFrame(image.getHeight(), image.getWidth(), CV_8UC1, (void *)image.getPixels());
                processRawFrame(rawFrame, outputFrame, format);
                if (!outputFrame.empty())
                {
                    // Resize to a fixed resolution.
                    cv::Mat resizedFrame;
                    cv::resize(outputFrame, resizedFrame, cv::Size(640, 480));

                    // Update FPS calculation.
                    // ToDo is this correct ?
                    frameCount++;
                    auto currentTime = std::chrono::steady_clock::now();
                    double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count() / 1000.0;
                    double fps = (elapsed > 0) ? frameCount / elapsed : 0.0;
                    if (elapsed >= 1.0)
                    {
                        lastTime = currentTime;
                        frameCount = 0;
                    }

                    // Prepare overlay text.
                    uint64_t timestampNS = buffer->getTimestampNS();
                    double timestampSec = static_cast<double>(timestampNS) / 1e9;
                    std::ostringstream oss;
                    oss << "TS: " << std::fixed << std::setprecision(6) << timestampSec << " s"
                        << " | FPS: " << std::fixed << std::setprecision(2) << fps;
                    std::ostringstream camOss;
                    camOss << "Cam: " << device->getID();

                    // Use cv::getTextSize to compute proper placement so text is inside the image.
                    int baseline = 0;
                    cv::Size textSize = cv::getTextSize(oss.str(), cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
                    int textY = resizedFrame.rows - baseline - 5; // 5-pixel margin above bottom.
                    cv::putText(resizedFrame, oss.str(), cv::Point(10, textY),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);

                    // Add camera ID in a separate line.
                    cv::putText(resizedFrame, camOss.str(), cv::Point(10, textY - textSize.height - 10),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);

                    // Update the global frame for this camera.
                    {
                        std::lock_guard<std::mutex> lock(globalFrameMutex);
                        globalFrames[index] = resizedFrame.clone();
                    }
                }
                else
                {
                    if (debug)
                        std::cerr << "[DEBUG] Empty output frame for camera " << device->getID() << std::endl;
                }
            }
            else
            {
                std::cerr << YELLOW << "Camera " << device->getID() << ": Invalid image grabbed." << RESET << std::endl;
            }
        }

        // Cleanup.
        try
        {
            stream->stopStreaming();
            stream->close();
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Exception during stream cleanup for camera " << device->getID()
                      << ": " << ex.what() << RESET << std::endl;
            return;
        }
        device->close();
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Exception in camera " << device->getID() << " thread: "
                  << ex.what() << RESET << std::endl;
        // Cleanup.
        try
        {
            device->close();
        }
        catch (const std::exception &ex)
        {
            std::cerr << RED << "Exception during stream cleanup for camera " << device->getID()
                      << ": " << ex.what() << RESET << std::endl;
            return;
        }
        return;
    }
}

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

std::string getCurrentIP(const std::shared_ptr<rcg::Device> &device)
{
  device->open(rcg::Device::CONTROL);
  auto nodemap = device->getRemoteNodeMap();
  try
  {
    std::string currentIP = rcg::getString(nodemap, "GevCurrentIPAddress");
    if (!isDottedIP(currentIP))
    {
      try
      {
        if (currentIP.find_first_not_of("0123456789") == std::string::npos)
        {
          currentIP = convertDecimalToIP(std::stoul(currentIP));
        }
        else
        {
          currentIP = convertHexToIP(currentIP);
        }
      }
      catch (const std::invalid_argument &e)
      {
        std::cerr << "Invalid IP address format: " << currentIP << std::endl;
        currentIP = "";
      }
    }

    device->close();
    return currentIP;
  }
  catch (const std::exception &ex)
  {
    device->close();
    std::cerr << "Exception: " << ex.what() << std::endl;
    return "";
  }
}

// ToDo add a ptpcheck method that checks if ptp is true on all devices and there are masters and slave it 
void enablePTP(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures)
{
    std::string feature = deprecatedFeatures ? "GevIEEE1588" : "PtpEnable";

    try
    {
        rcg::setString(nodemap, feature.c_str(), "true", true);
        ptpConfig.enabled = rcg::getBoolean(nodemap, feature.c_str());
        if (debug)
        {
            std::cout << GREEN << "PTP enable success" << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to enable PTP: " << ex.what() << RESET << std::endl;
    }
}

void getPtpParameters(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures)
{
    try
    {
        if (!deprecatedFeatures)
        {
            rcg::callCommand(nodemap, "TimestampLatch");
            rcg::callCommand(nodemap, "PtpDataSetLatch");
            ptpConfig.enabled = rcg::getBoolean(nodemap, "PtpEnable");
            ptpConfig.status = rcg::getString(nodemap, "PtpStatus");
            ptpConfig.clockAccuracy = rcg::getString(nodemap, "PtpClockAccuracy");
            ptpConfig.offsetFromMaster = rcg::getInteger(nodemap, "PtpOffsetFromMaster");
        }
        else
        {
            rcg::callCommand(nodemap, "GevIEEE1588");
            ptpConfig.enabled = rcg::getBoolean(nodemap, "GevIEEE1588");
            ptpConfig.status = rcg::getString(nodemap, "GevIEEE1588Status");
            ptpConfig.clockAccuracy = "Unavailable";
            ptpConfig.offsetFromMaster = -1;
        }
        if (debug)
        {
            std::cout << GREEN << "PTP Parameters success " << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get PTP parameters: " << ex.what() << RESET << std::endl;
    }
}

void statusCheck(const std::string &current_status)
{
    if (current_status == "Initializing")
    {
        num_init++;
    }
    else if (current_status == "Master")
    {
        num_master++;
    }
    else if (current_status == "Slave")
    {
        num_slave++;
    }
    else
    {
        std::cerr << RED << "Unknown PTP status: " << current_status << RESET << std::endl;
    }
}

// Calculate packet delay (not used in composite display but provided for completeness).
double CalculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams)
{
    double buffer = (bufferPercent / 100.0) * (packetSizeB * (1e9 / deviceLinkSpeedBps));
    return (((packetSizeB * (1e9 / deviceLinkSpeedBps)) + buffer) * (numCams - 1));
}

// Calculate packet delay (not used in composite display but provided for completeness).
double CalculateTransmissionDelayNs(double packetDelayNs, int camIndex)
{
    return packetDelayNs * camIndex;
}

// Set bandwidth parameters on the device.
void setBandwidth(const std::shared_ptr<rcg::Device> &device, double camIndex)
{
    double deviceLinkSpeedBps = 125000000; // 1 Gbps // ToDo What does this mean and how to define this
    double packetSizeB = 8228;             // ToDo What does this mean and how to define this, jumbo frames?
    double bufferPercent = 15;             // 10.93;

    try
    {
        device->open(rcg::Device::CONTROL);
        auto nodemap = device->getRemoteNodeMap();
        double packetDelay = CalculatePacketDelayNs(packetSizeB, deviceLinkSpeedBps, bufferPercent, numCams);
        if (debug)
            std::cout << "[DEBUG] numCams and CamIndex: " << numCams << " " << camIndex << std::endl;
        std::cout << "[DEBUG] Camera " << device->getID() << ": Calculated packet delay: " << packetDelay << " ns" << std::endl;
        double transmissionDelay = CalculateTransmissionDelayNs(packetDelay, camIndex);
        if (debug)
            std::cout << "[DEBUG] Camera " << device->getID() << ": Calculated transmission delay: " << transmissionDelay << " ns" << std::endl;
        int64_t gevSCPDValue = static_cast<int64_t>(packetDelay);
        double gevSCPDValueMs = gevSCPDValue / 1e6;

        rcg::setInteger(nodemap, "GevSCPD", gevSCPDValue);
        if (debug)
            std::cout << "[DEBUG] Camera " << device->getID() << ": GevSCPD set to: " << rcg::getInteger(nodemap, "GevSCPD") << " ns" << std::endl;
        if (device->getID() == "devicemodul04_5d_4b_79_71_12") // Example: skip specific device.
        {
            rcg::setInteger(nodemap, "GevSCPD", gevSCPDValueMs);
        }
        int64_t gevSCFTDValue = static_cast<int64_t>(transmissionDelay);
        double gevSCFTDValueMs = gevSCFTDValue / 1e6;
        if (debug)
            std::cout << "[DEBUG] Camera " << device->getID() << ": GevSCFTD in seconds: " << gevSCFTDValueMs << " s" << std::endl;
        rcg::setInteger(nodemap, "GevSCFTD", gevSCFTDValue);
        if (device->getID() == "devicemodul04_5d_4b_79_71_12") // Example: skip specific device.
        {
            rcg::setEnum(nodemap, "ImageTransferDelayMode", "On");
            rcg::setFloat(nodemap, "ImageTransferDelayValue", gevSCFTDValueMs);
        }

        if (debug)
            std::cout << "[DEBUG] Camera " << device->getID() << ": GevSCFTD set to: " << rcg::getInteger(nodemap, "GevSCFTD") << " ns" << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Failed to set bandwidth for camera " << device->getID() << ": " << ex.what() << RESET << std::endl;
    }
}


bool isDottedIP(const std::string &ip)
{
  std::regex ipPattern(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
  std::smatch match;

  if (std::regex_match(ip, match, ipPattern))
  {
    for (int i = 1; i <= 4; ++i)
    {
      int octet = std::stoi(match[i].str());
      if (octet < 0 || octet > 255)
        return false;
    }
    return true;
  }
  return false;
}

std::string convertDecimalToIP(uint32_t decimalIP)
{
  struct in_addr ip_addr;
  ip_addr.s_addr = htonl(3232236391);
  return std::string(inet_ntoa(ip_addr));
}

std::string convertHexToIP(const std::string &hexIP)
{
  uint32_t decimalIP = std::stoul(hexIP, nullptr, 16);
  return convertDecimalToIP(decimalIP);
}

bool isMacAddress(const std::string &mac)
{
  std::regex macPattern(R"(^([0-9A-Fa-f]{2}:){5}([0-9A-Fa-f]{2})$)");
  return std::regex_match(mac, macPattern);
}

std::string convertDecimalToMAC(uint64_t decimalMAC)
{
  std::ostringstream macStream;
  for (int i = 5; i >= 0; --i)
  {
    macStream << std::hex << std::setw(2) << std::setfill('0') << ((decimalMAC >> (i * 8)) & 0xFF);
    if (i > 0)
      macStream << ":";
  }
  return macStream.str();
}

std::string convertHexToMAC(const std::string &hexMAC)
{
  uint64_t decimalMAC = std::stoull(hexMAC, nullptr, 16);
  return convertDecimalToMAC(decimalMAC);
}

std::string getMAC(const std::shared_ptr<rcg::Device> &device)
{
  device->open(rcg::Device::CONTROL);
  auto nodemap = device->getRemoteNodeMap();
  try
  {
    std::string MAC = rcg::getString(nodemap, "GevMACAddress");
    if (!isMacAddress(MAC))
    {
      try
      {
        if (MAC.find_first_not_of("0123456789") == std::string::npos)
        {
          uint64_t decimalMAC = std::stoull(MAC);
          MAC = convertDecimalToMAC(decimalMAC);
        }
        else
        {
          MAC = convertHexToMAC(MAC);
        }
      }
      catch (const std::invalid_argument &e)
      {
        std::cerr << "Invalid MAC address format: " << MAC << std::endl;
        MAC = "";
      }
    }
    device->close();
    return MAC;
  }
  catch (const std::exception &ex)
  {
    device->close();
    std::cerr << "Exception: " << ex.what() << std::endl;
    return "";
  }
}

void getTimestamps(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures)
{
    try
    {
        if (!deprecatedFeatures)
        {
            rcg::callCommand(nodemap, "TimestampLatch");
            ptpConfig.timestamp_ns = rcg::getString(nodemap, "TimestampLatchValue");
            ptpConfig.tickFrequency = "1000000000"; // Assuming 1 GHz if PTP is used
        }
        else
        {
            rcg::callCommand(nodemap, "GevTimestampControlLatch");
            ptpConfig.timestamp_ns = rcg::getString(nodemap, "GevTimestampValue");
            ptpConfig.tickFrequency = rcg::getString(nodemap, "GevTimestampTickFrequency"); // if PTP is used, this feature must return 1,000,000,000 (1 GHz).
        }

        ptpConfig.timestamp_s = std::stoull(ptpConfig.timestamp_ns) / std::stoull(ptpConfig.tickFrequency);
        if (debug)
        {
            std::cout << GREEN << "Timestamp success" << RESET << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << RED << "Error: Failed to get timestamp: " << ex.what() << RESET << std::endl;
    }
}