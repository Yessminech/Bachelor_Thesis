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
  try
  {
    const char *defaultCtiPath = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib"; // ToDo add other path mvImpact
    if (defaultCtiPath == nullptr)
    {
      std::cerr << RED << "Environment variable GENICAM_GENTL64_PATH is not set." << RESET << std::endl;
      return 1;
    }
    rcg::System::setSystemsPath(defaultCtiPath, nullptr);
    std::vector<std::shared_ptr<rcg::System>> defaultSystems = rcg::System::getSystems();
    if (defaultSystems.empty())
    {
      std::cerr << RED << "Error: No systems found." << RESET << std::endl;
      return 1;
    }
    for (auto &system : defaultSystems)
    {
      system->open();
      std::vector<std::shared_ptr<rcg::Interface>> interfs = system->getInterfaces();
      if (interfs.empty())
      {
        continue;
      }
      for (auto &interf : interfs)
      {
        interf->open();
        std::vector<std::shared_ptr<rcg::Device>> devices = interf->getDevices();
        if (devices.empty())
        {
          continue;
        }

        for (auto &device : devices)
        {

            std::string serialNumber = device->getSerialNumber();
            if (printedSerialNumbers.find(serialNumber) == printedSerialNumbers.end())
            {
              printedSerialNumbers.insert(serialNumber);
              std::cout << "Device ID: " << device->getID() << std::endl;
            }
          }
          interf->close();
        }
        system->close();
      }
  }
  catch (const std::exception &ex)
  {
    std::cout << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
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
    system[i]->~System();
  }
  try
  {
    const char *defaultCtiPath = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib"; // ToDo add other path mvImpact
    if (defaultCtiPath == nullptr)
    {
      std::cerr << RED << "Environment variable GENICAM_GENTL64_PATH is not set." << RESET << std::endl;
      return 1;
    }
    rcg::System::setSystemsPath(defaultCtiPath, nullptr);
    std::vector<std::shared_ptr<rcg::System>> defaultSystems = rcg::System::getSystems();
    if (defaultSystems.empty())
    {
      std::cerr << RED << "Error: No systems found." << RESET << std::endl;
      return 1;
    }
    for (auto &system : defaultSystems)
    {
      system->open();
      std::vector<std::shared_ptr<rcg::Interface>> interfs = system->getInterfaces();
      if (interfs.empty())
      {
        continue;
      }
      for (auto &interf : interfs)
      {
        interf->open();
        std::vector<std::shared_ptr<rcg::Device>> devices = interf->getDevices();
        if (devices.empty())
        {
          continue;
        }

        for (auto &device : devices)
        {

            std::string serialNumber = device->getSerialNumber();
            if (printedSerialNumbers.find(serialNumber) == printedSerialNumbers.end())
            {
              printedSerialNumbers.insert(serialNumber);
              std::cout << "Device ID: " << device->getID() << std::endl;
            }
          device->close();
        }
        interf->close();
      }
      system->close();
    }
  }
  catch (const std::exception &ex)
  {
    std::cout << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
  }
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
      if (debug)
      {
        std::cout << "System Path: " << system[i]->getDisplayName() << std::endl;
      }
      system[i]->open();
      std::vector<std::shared_ptr<rcg::Interface>> interf = system[i]->getInterfaces();
      for (size_t k = 0; k < interf.size(); k++)
      {
        interf[k]->open();
        std::vector<std::shared_ptr<rcg::Device>> device = interf[k]->getDevices();
        if (device.empty())
        {
          if (debug)
          {
            std::cout << "No devices found on this interface. Please try again in a moment." << std::endl;
          }
          continue;
        }

        for (size_t j = 0; j < device.size(); j++)
        {

          std::string deviceVendor = device[j]->getVendor();
          if (debug)
          {
            std::cout << "System Vendor: " << systemVendor << std::endl;
            std::cout << "Device Vendor: " << deviceVendor << std::endl;
          }
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
    system[i]->~System();
  }
  try
  {
    const char *defaultCtiPath = "/home/test/Downloads/Baumer_GAPI_SDK_2.15.2_lin_x86_64_cpp/lib"; // ToDo add other path mvImpact
    if (defaultCtiPath == nullptr)
    {
      std::cerr << RED << "Environment variable GENICAM_GENTL64_PATH is not set." << RESET << std::endl;
      return 1;
    }
    rcg::System::setSystemsPath(defaultCtiPath, nullptr);
    std::vector<std::shared_ptr<rcg::System>> defaultSystems = rcg::System::getSystems();
    if (defaultSystems.empty())
    {
      std::cerr << RED << "Error: No systems found." << RESET << std::endl;
      return 1;
    }
    for (auto &system : defaultSystems)
    {
      system->open();
      std::vector<std::shared_ptr<rcg::Interface>> interfs = system->getInterfaces();
      if (interfs.empty())
      {
        continue;
      }
      for (auto &interf : interfs)
      {
        interf->open();
        std::vector<std::shared_ptr<rcg::Device>> devices = interf->getDevices();
        if (devices.empty())
        {
          continue;
        }

        for (auto &device : devices)
        {

            std::string serialNumber = device->getSerialNumber();
            if (printedSerialNumbers.find(serialNumber) == printedSerialNumbers.end())
            {
              printedSerialNumbers.insert(serialNumber);
              std::cout << "Device ID: " << device->getID() << std::endl;
            }
          }
          interf->close();

        }
        system->close();

      }
  }
  catch (const std::exception &ex)
  {
    std::cout << RED << "Error: Exception: " << ex.what() << RESET << std::endl;
  }
  return ret;
}

int main(int argc, char *argv[])
{
  int ret = 0;

  try
  {
    listDevices();
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Exception: " << ex.what() << std::endl;
    ret = 2;
  }
  return ret;
}

// ToDo: Solve bug that cams can't always be listed when the program is run multiple times
//ToDo: correct default cases 