#include <iostream>
#include <GenTL/GenTL_v1_6.h>

uint32_t NumInterfaces = 0;
char InterfaceID[256];
size_t InterfaceIDSize = sizeof(InterfaceID);
GenTL::IF_HANDLE hNewIface;
GenTL::TL_HANDLE hTL;
GenTL::GC_ERROR success ; 

int main()
{
    // Initialize the library
    GenTL::GCInitLib();
    // Open the first transport layer
    GenTL::TLOpen(&hTL);
    // Open the first interface
    GenTL::TLUpdateInterfaceList(hTL, nullptr, 1000);
    GenTL::TLGetNumInterfaces(hTL, &NumInterfaces);
    if (NumInterfaces > 0)
    {
        for (uint32_t i = 0; i < NumInterfaces; i++)
        {
            // First query the buffer size
            GenTL::TLGetInterfaceID(hTL, i, InterfaceID, &InterfaceIDSize);
            std::cout << "Interface ID: " << InterfaceID << std::endl;
        }
    }
    else
    {
        std::cerr << "No interfaces found" << std::endl;
        return 1;
    }

    // Close the transport layer
    GenTL::TLClose(hTL);

    // Close the library
    GenTL::GCCloseLib();
}