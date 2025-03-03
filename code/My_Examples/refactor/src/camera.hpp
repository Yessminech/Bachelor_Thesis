struct DeviceConfig
{
  std::string id;
  std::string vendor;
  std::string model;
  std::string tlType;
  std::string displayName;
  std::string accessStatus;
  std::string serialNumber;
  std::string version;
  std::string currentIP;
  std::string MAC;
  double timestampFrequency;
};

struct PTPConfig
{
    bool enabled;
    std::string status;
    std::string timestamp_ns;
    std::string tickFrequency;
    uint64_t timestamp_s;
    std::string clockAccuracy;
    int offsetFromMaster;
};

DeviceConfig getConfig(const std::shared_ptr<rcg::Device> &device);
bool getGenTLVersion(std::shared_ptr<GenApi::CNodeMapRef> nodemap);
void enableChunkData(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID);
void configureSyncFreeRun(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID);
void setupCameraSettings(const std::shared_ptr<GenApi::CNodeMapRef> &nodemap, const std::string &camID);
void displayCameraStream(const std::shared_ptr<rcg::Device> &device, int index);
void configureActionCommandDevice(std::shared_ptr<rcg::Device> device, uint32_t actionDeviceKey, uint32_t groupKey, uint32_t groupMask, const char *triggerSource = "Action1", uint32_t actionSelector = 1);
std::string getCurrentIP(const std::shared_ptr<rcg::Device> &device);
void enablePTP(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures);
void getPtpParameters(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures);
void statusCheck(const std::string &current_status);
double CalculatePacketDelayNs(double packetSizeB, double deviceLinkSpeedBps, double bufferPercent, double numCams);
double CalculateTransmissionDelayNs(double packetDelayNs, int camIndex);
void setBandwidth(const std::shared_ptr<rcg::Device> &device, double camIndex)
std::string convertDecimalToIP(uint32_t decimalIP);
std::string convertHexToIP(const std::string &hexIP);
bool isMacAddress(const std::string &mac);
std::string convertDecimalToMAC(uint64_t decimalMAC);
std::string convertHexToMAC(const std::string &hexMAC);
std::string getMAC(const std::shared_ptr<rcg::Device> &device);
void getTimestamps(std::shared_ptr<GenApi::CNodeMapRef> nodemap, PTPConfig &ptpConfig, bool deprecatedFeatures);