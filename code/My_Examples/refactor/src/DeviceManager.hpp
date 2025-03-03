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

#define RESET "\033[0m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"

double numCams; // Todo Correct
std::vector<std::shared_ptr<rcg::Device>> globalDevices;

bool listDevicesByIdOrIP(const std::string &id = "", const std::string &ip = "");
bool listDevices();
bool listDevicesIDs();

// + addCamera(cameraId, config)   // Adds a new camera with configuration
// + removeCamera(cameraId)        // Removes a camera from the system
// + getCameraList() : List<Camera> // Returns a list of available cameras
// + getCameraStatus(cameraId) : CameraStatus // Retrieves the health/status of a specific camera
// + configureCamera(cameraId, params) // Configures a camera’s settings