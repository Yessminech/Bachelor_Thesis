#include <rc_genicam_api/system.h>
#include <rc_genicam_api/interface.h>
#include <rc_genicam_api/device.h>

#include <iostream>


int main(int argc, char *argv[])
{
  int ret=0;

  try
  {

        // list all systems, interfaces and devices

        std::vector<std::shared_ptr<rcg::System> > system=rcg::System::getSystems();

        for (size_t i=0; i<system.size(); i++)
        {
          system[i]->open();            
          std::vector<std::shared_ptr<rcg::Interface> > interf=system[i]->getInterfaces();
          for (size_t k=0; k<interf.size(); k++)
          {
            interf[k]->open();
            std::vector<std::shared_ptr<rcg::Device> > device=interf[k]->getDevices();
            for (size_t j=0; j<device.size(); j++)
            {
              std::cout << "        Device             " << device[j]->getID() << std::endl;
              std::cout << "        Vendor:            " << device[j]->getVendor() << std::endl;
              std::cout << "        Model:             " << device[j]->getModel() << std::endl;
              std::cout << "        TL type:           " << device[j]->getTLType() << std::endl;
              std::cout << "        Display name:      " << device[j]->getDisplayName() << std::endl;
              std::cout << "        User defined name: " << device[j]->getUserDefinedName() << std::endl;
              std::cout << "        Access status:     " << device[j]->getAccessStatus() << std::endl;
              std::cout << "        Serial number:     " << device[j]->getSerialNumber() << std::endl;
              std::cout << "        Version:           " << device[j]->getVersion() << std::endl;
              std::cout << "        TS Frequency:      " << device[j]->getTimestampFrequency() << std::endl;
              std::cout << std::endl;
            }

            interf[k]->close();
            }
          system[i]->close();
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        ret=2;
    }

  rcg::System::clearSystems();

  return ret;
}
