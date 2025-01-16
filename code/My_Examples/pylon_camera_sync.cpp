#include <pylon/PylonIncludes.h>
#include <pylon/InstantCameraArray.h>

using namespace Pylon;
using namespace GenApi;

/*Minimal Example for using Pylon SDK for Camera Synchronisation with action commands*/

int main()
{
    // Initialize Pylon runtime
    PylonInitialize();

    try
    {
        // Create an array to hold multiple cameras
        CInstantCameraArray cameras;
        cameras.Initialize();

        // Attach all connected cameras to the array
        cameras.AttachAllDevices();

        // Ensure you have at least 2 cameras
        if (cameras.GetSize() < 2)
        {
            throw RUNTIME_EXCEPTION("At least two cameras are required.");
        }

        // --- Start of camera setup ---
        for (size_t i = 0; i < cameras.GetSize(); ++i)
        {
            // Open the camera connection
            cameras[i].Open();

            // Select and enable the Frame Start trigger
            cameras[i].TriggerSelector.SetValue(TriggerSelector_FrameStart);
            cameras[i].TriggerMode.SetValue(TriggerMode_On);

            // Set the source for the Frame Start trigger to Action 1
            cameras[i].TriggerSource.SetValue(TriggerSource_Action1);

            // Specify the action device key and action group key
            cameras[i].ActionDeviceKey.SetValue(4711); // Must match the command
            cameras[i].ActionGroupKey.SetValue(1);

            // Specify the action group mask (all cameras respond to this)
            cameras[i].ActionGroupMask.SetValue(0xffffffff);
        }
        // --- End of camera setup ---

        // --- Issue an action command ---
        GigeTL->IssueActionCommand(4711, 1, 0xffffffff, "192.168.1.255");

        // Close the cameras
        for (size_t i = 0; i < cameras.GetSize(); ++i)
        {
            cameras[i].Close();
        }
    }
    catch (const GenericException &e)
    {
        // Handle exceptions
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
        PylonTerminate();
        return 1;
    }

    // Terminate Pylon runtime
    PylonTerminate();
    return 0;
}
