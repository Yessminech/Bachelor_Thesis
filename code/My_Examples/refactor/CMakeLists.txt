cmake_minimum_required(VERSION 3.10)
project(genicam_example)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SOURCES
    src/GlobalSettings.cpp
    src/NetworkManager.cpp
    src/Camera.cpp
    src/DeviceManager.cpp
    src/StreamManager.cpp
    src/SystemManager.cpp
    src/main.cpp
)

find_package(OpenCV REQUIRED)

# ToDo Take off useless lines 
include_directories("/usr/include/rc_genicam_api")
include_directories("/usr/include/rc_genicam_api/genicam")
include_directories("~/Downloads/ArenaSDK_v0.1.91_Linux_x64/ArenaSDK_Linux_x64/include")
include_directories(${OpenCV_INCLUDE_DIRS})

# Manually add the directory containing the library.
link_directories("/usr/lib/x86_64-linux-gnu")
link_directories("~/Downloads/ArenaSDK_v0.1.91_Linux_x64/ArenaSDK_Linux_x64/lib64")

# Create the executable
add_executable(globalSettings ${SOURCES})
add_executable(streamManager ${SOURCES})
add_executable(deviceManager ${SOURCES})
add_executable(networkManager ${SOURCES})
add_executable(systemManager ${SOURCES})
add_executable(main ${SOURCES})

# Define the symbol to disable automatic import library linking.
target_compile_definitions(globalSettings PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(streamManager PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(deviceManager PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(networkManager PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(systemManager PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(main PRIVATE GENICAM_NO_AUTO_IMPLIB)

# Link the executable with the rc_genicam_api library.
# The installed shared library is located at /usr/lib/x86_64-linux-gnu/librc_genicam_api.so.
target_link_libraries(globalSettings PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(streamManager PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(deviceManager PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(networkManager PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(systemManager PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(main PRIVATE rc_genicam_api ${OpenCV_LIBS})
