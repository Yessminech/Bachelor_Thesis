#!/bin/bash

# Set the GENICAM_GENTL64_PATH environment variable
export GENICAM_GENTL64_PATH=/opt/pylon/lib/gentlproducer/gtl
echo "GENICAM_GENTL64_PATH set to $GENICAM_GENTL64_PATH"

# Configure the IP address for the network interface
sudo ip addr add 192.168.3.1/24 dev enp3s0f0
echo "IP address 192.168.3.1/24 added to enp3s0f0"

# chmod +x config.sh
# ./config.sh