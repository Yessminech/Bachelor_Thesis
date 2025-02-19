#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <sstream>
#include <set>
#include <arpa/inet.h>
#include <iomanip>
#include <regex>
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

bool listDevicesByIdOrIP(const std::string &id = "", const std::string &ip = "")
{
  bool ret = true;
  std::set<std::string> printedSerialNumbers;

  std::vector<std::shared_ptr<rcg::System>> system = rcg::System::getSystems(); // ToDo skip not gige
  for (size_t i = 0; i < system.size(); i++)
  {
    try
    {
      std::string systemVendor = system[i]->getVendor();


      system[i]->open();
      std::vector<std::shared_ptr<rcg::Interface>> interf = system[i]->getInterfaces();
      for (size_t k = 0; k < interf.size(); k++)
      {
        interf[k]->open();
        std::vector<std::shared_ptr<rcg::Device>> device = interf[k]->getDevices();
        for (size_t j = 0; j < device.size(); j++)
        {
          std::string deviceVendor = device[j]->getVendor();
          if (deviceVendor == systemVendor)
          {
            auto nodemap = device[j]->getRemoteNodeMap();
            std::string serialNumber = device[j]->getSerialNumber();
            std::string currentIP = getCurrentIP(device[j]);
            if ((id.empty() || device[j]->getID() == id) && (ip.empty() || currentIP == ip))
            {
              if (printedSerialNumbers.find(serialNumber) == printedSerialNumbers.end())
              {
                printedSerialNumbers.insert(serialNumber);
                DeviceConfig config = getConfig(device[j]);
                std::cout << "        Device             " << config.id << std::endl;
                std::cout << "        Vendor:            " << config.vendor << std::endl;
                std::cout << "        Model:             " << config.model << std::endl;
                std::cout << "        TL type:           " << config.tlType << std::endl;
                std::cout << "        Display name:      " << config.displayName << std::endl;
                std::cout << "        Access status:     " << config.accessStatus << std::endl;
                std::cout << "        Serial number:     " << config.serialNumber << std::endl;
                std::cout << "        Version:           " << config.version << std::endl;
                std::cout << "        TS Frequency:      " << config.timestampFrequency << std::endl;
                std::cout << "        Current IP:        " << config.currentIP << std::endl;
                std::cout << "        MAC:               " << config.MAC << std::endl;
                std::cout << std::endl;
              }
            }
          }
        }
        interf[k]->close();
      }
    }
    catch (const std::exception &ex)
    {
      std::cerr << "Exception: " << ex.what() << std::endl;
      system[i]->close();
      return false;
    }
    system[i]->close();
    system[i]->clearSystems();
    system[i]->~System();
  }
  return ret;
}

bool listDevices()
{ // ToDo: Exceptions handling
  bool ret = true;
  std::set<std::string> printedSerialNumbers;

  std::vector<std::shared_ptr<rcg::System>> system = rcg::System::getSystems();
  for (size_t i = 0; i < system.size(); i++)
  {
    try
    {
      std::string systemVendor = system[i]->getVendor();
      system[i]->open();
      std::vector<std::shared_ptr<rcg::Interface>> interf = system[i]->getInterfaces();
      for (size_t k = 0; k < interf.size(); k++)
      {
        interf[k]->open();
        std::vector<std::shared_ptr<rcg::Device>> device = interf[k]->getDevices();
        for (size_t j = 0; j < device.size(); j++)
        {
          std::string deviceVendor = device[j]->getVendor();
          if (deviceVendor == systemVendor)
          {
            auto nodemap = device[j]->getRemoteNodeMap();
            std::string serialNumber = device[j]->getSerialNumber();
            if (printedSerialNumbers.find(serialNumber) == printedSerialNumbers.end())
            {
              printedSerialNumbers.insert(serialNumber);
              DeviceConfig config = getConfig(device[j]);
              std::cout << "        Device             " << config.id << std::endl;
              std::cout << "        Vendor:            " << config.vendor << std::endl;
              std::cout << "        Model:             " << config.model << std::endl;
              std::cout << "        TL type:           " << config.tlType << std::endl;
              std::cout << "        Display name:      " << config.displayName << std::endl;
              std::cout << "        Access status:     " << config.accessStatus << std::endl;
              std::cout << "        Serial number:     " << config.serialNumber << std::endl;
              std::cout << "        Version:           " << config.version << std::endl;
              std::cout << "        TS Frequency:      " << config.timestampFrequency << std::endl;
              std::cout << "        Current IP:        " << config.currentIP << std::endl;
              std::cout << "        MAC:               " << config.MAC << std::endl;
              std::cout << std::endl;
            }
          }
        }
        interf[k]->close();
      }
    }
    catch (const std::exception &ex)
    {
      std::cerr << "Exception: " << ex.what() << std::endl;
      system[i]->close();
      return false;
    }
    system[i]->close();
    system[i]->clearSystems();
    system[i]->~System();  }
  return ret;
}

// Function to list all available devices
bool listDevicesIDs()
{ // ToDo: Exceptions handling
  bool ret = true;
  std::set<std::string> printedSerialNumbers;

  std::vector<std::shared_ptr<rcg::System>> system = rcg::System::getSystems();
  for (size_t i = 0; i < system.size(); i++)
  {
    try
    {
      std::string systemVendor = system[i]->getVendor();
      std::cout << "System Path: " << system[i]->getDisplayName() << std::endl;
      system[i]->open();
      std::vector<std::shared_ptr<rcg::Interface>> interf = system[i]->getInterfaces();
      for (size_t k = 0; k < interf.size(); k++)
      {
        interf[k]->open();
        std::vector<std::shared_ptr<rcg::Device>> device = interf[k]->getDevices();
        if (device.empty())
        {
          std::cout << "No devices found on this interface. Please try again in a moment." << std::endl;
        }

        for (size_t j = 0; j < device.size(); j++)
        {

          std::string deviceVendor = device[j]->getVendor();

          std::cout << "System Vendor: " << systemVendor << std::endl;  
          std::cout << "Device Vendor: " << deviceVendor << std::endl;
          if (deviceVendor == systemVendor)
          {
            std::string serialNumber = device[j]->getSerialNumber();
            if (printedSerialNumbers.find(serialNumber) == printedSerialNumbers.end())
            {
              printedSerialNumbers.insert(serialNumber);

              std::cout << "Device ID: " << device[j]->getID() << std::endl;
            }
          }
        }
        interf[k]->close();
      }
    }
    catch (const std::exception &ex)
    {
      std::cerr << "Exception: " << ex.what() << std::endl;
      system[i]->close();
      system[i]->~System();    
      return false;
    }
    system[i]->close();
    system[i]->clearSystems();
    system[i]->~System();  }
  return ret;
}

int main(int argc, char *argv[])
{
  int ret = 0;

  try
  {
    if (!listDevicesIDs())
    {
      std::cerr << "No devices found!" << std::endl;
      ret = 1;
    }
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Exception: " << ex.what() << std::endl;
    ret = 2;
  }
  return ret;
}

// ToDo: Solve bug that cams can't always be listed when the program is run multiple times
// ToDo add default case using baumer and mvimpact