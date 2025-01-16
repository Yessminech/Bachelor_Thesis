#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>
#include <iostream>

/**
 * @brief Entry point of the application.
 *
 * This function retrieves and prints information about available GenICam systems, interfaces, and devices.
 * It performs the following steps:
 * 1. Retrieves a list of available GenICam systems.
 * 2. For each system, it opens the system and retrieves a list of interfaces.
 * 3. For each interface, it opens the interface and retrieves a list of devices.
 * 4. For each device, it prints the device ID to the standard output.
 * 5. Closes each interface and system after processing.
 * 6. Clears the list of systems to release resources.
 *
 * If any exception is thrown during the process, it catches the exception, prints the error message to the standard error output,
 * and returns a non-zero value to indicate failure.
 *
 * @return int Returns 0 on success, or 1 if an exception is caught.
 */
int main()
{
  try
  {
    auto systems = rcg::System::getSystems();
    for (const auto& system : systems)
    {
      system->open();
      auto interfaces = system->getInterfaces();
      for (const auto& interf : interfaces)
      {
        interf->open();
        auto devices = interf->getDevices();
        for (const auto& device : devices)
        {
          std::cout << "Device ID: " << device->getID() << std::endl;
        }
        interf->close();
      }
      system->close();
    }
    rcg::System::clearSystems();
  }
  catch (const std::exception &ex)
  {
    std::cerr << ex.what() << std::endl;
    return 1;
  }
  return 0;
}
