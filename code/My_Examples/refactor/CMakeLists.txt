cmake_minimum_required(VERSION 3.10)
project(genicam_example)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

include_directories("/usr/include/rc_genicam_api")
include_directories("/usr/include/rc_genicam_api/genicam")
include_directories("~/Downloads/ArenaSDK_v0.1.91_Linux_x64/ArenaSDK_Linux_x64/include")
include_directories(${OpenCV_INCLUDE_DIRS})

# Manually add the directory containing the library.
link_directories("/usr/lib/x86_64-linux-gnu")
link_directories("~/Downloads/ArenaSDK_v0.1.91_Linux_x64/ArenaSDK_Linux_x64/lib64")
# Create the executable
add_executable(camera src/camera.cpp)

# Define the symbol to disable automatic import library linking.
target_compile_definitions(camera PRIVATE GENICAM_NO_AUTO_IMPLIB)

# Link the executable with the rc_genicam_api library.
# The installed shared library is located at /usr/lib/x86_64-linux-gnu/librc_genicam_api.so.
target_link_libraries(camera PRIVATE rc_genicam_api )
