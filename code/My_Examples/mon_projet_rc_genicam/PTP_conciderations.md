## Delay Calculator
Goal: Avoid Congestion in a multiple camera setup 
Delay Type 1: Stream Channel Packet Delay (GevSCPD)
﻿﻿// Determine the current GevSCPD
int64_t value = nodeMapRemoteDevice->FindNode<peak::core::nodes::IntegerNode>("GevSCPD")->Value();
﻿// Set GevSCPD to 0
nodeMapRemoteDevice->FindNode<peak::core::nodes::IntegerNode>("GevSCPD")->SetValue(0);



“PTP Sync Mode” is LUCID’s name for a feature that allows cameras to automatically synchronize their frame rates over PTP. // ToDo implement this in the AcquisitionStartMode to dynamically adjus frame rates to PTPSyncFrameRate
Synchronous Free Run // the same time and the same frame rate in SyncFreeRunTimerTriggerRateAbs

Executing a synchronized scheduled software trigger: 
1. Achieve clock synchronization. Set PtpMode = Master,
Slave or Auto.
2. Set camera TriggerSelector = FrameStart and
TriggerSource = Software.
3. Call AcquisitionStart() to start an acquisition
stream.
4. Determine the current camera GevTimestampValue
using GevTimestampControlLatch.
5. Set PtpAcquisitionGateTime to a value that
sufficiently exceeds the current camera
GevTimestampValue.
6. Execute TriggerSoftware command.

Fixed frame rate synchronization
1. Synchronize camera clocks.
2. Set TriggerSelector = FrameStart and TriggerSource = FixedRate on all cameras.
3. Set AcquisitionFrameRateAbs to the same value on all cameras.
4. Set PtpAcquisitionGateTime the same for all cameras and greater than existing
GevTimestampValue.
5. Start streaming each camera
PtpAcquisitionGateTime = Current G evTimestampValue + delta, where delta  is the combined time required to set PtpAcquisitionGateTime on each camera (at minimum). This time varies depending on operating system, host, network hardware, and system load.

Delay: The Action Command is scheduled after a user defined time interval in ms. Minimum value: 20 ms.