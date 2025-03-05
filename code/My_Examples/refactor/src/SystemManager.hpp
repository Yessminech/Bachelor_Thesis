#ifndef SYSTEM_MANAGER_HPP
#define SYSTEM_MANAGER_HPP

#include <rc_genicam_api/device.h>

#include <vector>
#include <memory>

class SystemManager {
public:
    SystemManager();
    ~SystemManager();

    void initializeSystem(); // Initializes all managers and system components
    void shutdownSystem(); // Gracefully stops all operations
    void restartSystem(); // Restarts all services and reinitializes components
    // SystemStatus getSystemStatus() const; // Returns overall system health and status
    std::vector<std::shared_ptr<rcg::Device>> listDevices() const; // Returns a list of all connected devices
    // void configureSystem(const Params& params); // Allows high-level system configuration
    void updateSystem(); // Updates the system
};

#endif // SYSTEM_MANAGER_HPP