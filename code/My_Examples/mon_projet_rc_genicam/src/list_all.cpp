#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <rc_genicam_api/nodemap_out.h>

#include <iostream>
#include <set>

struct DeviceConfig
{
  std::string id;
  std::string vendor;
  std::string model;
  std::string tlType;
  std::string displayName;
  std::string userDefinedName;
  std::string accessStatus;
  std::string serialNumber;
  std::string version;
  std::string currentIP;
  std::string MAC;
  double timestampFrequency;
};

std::string getCurrentIP(const std::shared_ptr<rcg::Device> &device)
{
  device->open(rcg::Device::CONTROL);
  auto nodemap = device->getRemoteNodeMap();
  try
  {
    std::string currentIP = rcg::getString(nodemap, "GevCurrentIPAddress");
    device->close();
    return currentIP;
  }
  catch (const std::exception &ex)
  {
    device->close();
    return "";
  }
}

std::string getMAC(const std::shared_ptr<rcg::Device> &device)
{
  device->open(rcg::Device::CONTROL);
  auto nodemap = device->getRemoteNodeMap();
  try
  {
    std::string MAC = rcg::getString(nodemap, "GevMACAddress");
    device->close();
    return MAC;
  }
  catch (const std::exception &ex)
  {
    device->close();
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
  config.userDefinedName = device->getUserDefinedName();
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

  // list all systems, interfaces and devices
  std::vector<std::shared_ptr<rcg::System>> system = rcg::System::getSystems();
  for (size_t i = 0; i < system.size(); i++)
  {
    system[i]->open();
    std::vector<std::shared_ptr<rcg::Interface>> interf = system[i]->getInterfaces();
    for (size_t k = 0; k < interf.size(); k++)
    {
      interf[k]->open();
      std::vector<std::shared_ptr<rcg::Device>> device = interf[k]->getDevices();
      for (size_t j = 0; j < device.size(); j++)
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
            std::cout << "        User defined name: " << config.userDefinedName << std::endl;
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
    system[i]->close();
  }
  return ret;
}

bool listDevices()
{ // ToDo: Exceptions handling
  bool ret = true;
  std::set<std::string> printedSerialNumbers;

  // list all systems, interfaces and devices
  std::vector<std::shared_ptr<rcg::System>> system = rcg::System::getSystems();
  for (size_t i = 0; i < system.size(); i++)
  {
    system[i]->open();
    std::vector<std::shared_ptr<rcg::Interface>> interf = system[i]->getInterfaces();
    for (size_t k = 0; k < interf.size(); k++)
    {
      interf[k]->open();
      std::vector<std::shared_ptr<rcg::Device>> device = interf[k]->getDevices();
      for (size_t j = 0; j < device.size(); j++)
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
          std::cout << "        User defined name: " << config.userDefinedName << std::endl;
          std::cout << "        Access status:     " << config.accessStatus << std::endl;
          std::cout << "        Serial number:     " << config.serialNumber << std::endl;
          std::cout << "        Version:           " << config.version << std::endl;
          std::cout << "        TS Frequency:      " << config.timestampFrequency << std::endl;
          std::cout << "        Current IP:        " << config.currentIP << std::endl;
          std::cout << "        MAC:               " << config.MAC << std::endl;
          std::cout << std::endl;
        }
        else
        {
          std::cout << "        Looking...        " << std::endl;
        }
      }
      interf[k]->close();
    }
    system[i]->close();
  }
  return ret;
}

// Function to list all available devices
bool listDevicesIDs()
{ // ToDo: Exceptions handling
  bool ret = true;
  std::vector<std::shared_ptr<rcg::System>> system = rcg::System::getSystems();
  for (size_t i = 0; i < system.size(); i++)
  {
    system[i]->open();
    std::vector<std::shared_ptr<rcg::Interface>> interf = system[i]->getInterfaces();
    for (size_t k = 0; k < interf.size(); k++)
    {
      interf[k]->open();
      std::vector<std::shared_ptr<rcg::Device>> device = interf[k]->getDevices();
      for (size_t j = 0; j < device.size(); j++)
      {
        std::cout << "Device ID: " << device[j]->getID() << std::endl;
      }
      interf[k]->close();
    }
    system[i]->close();
  }
  return ret;
}

int main(int argc, char *argv[])
{
  int ret = 0;

  try
  {
    if (!listDevices())
    {
      ret = 1;
    }
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Exception: " << ex.what() << std::endl;
    ret = 2;
  }

  rcg::System::clearSystems();

  return ret;
}
