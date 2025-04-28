cmake_minimum_required(VERSION 3.10)
project(genicam_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)  # Enable Qt MOC for signals/slots if needed

find_package(OpenCV REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Widgets Core Gui PrintSupport)

# ToDo Take off useless lines 
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR})
include_directories("/usr/include/rc_genicam_api")
include_directories("/usr/include/rc_genicam_api/genicam")
include_directories("~/Downloads/ArenaSDK_v0.1.91_Linux_x64/ArenaSDK_Linux_x64/include") # ToDo Change the path to the correct one
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${Qt5Widgets_INCLUDE_DIRS} ${Qt5Core_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS})

# Manually add the directory containing the library.
link_directories("/usr/lib/x86_64-linux-gnu")

set(SOURCES
    src/GlobalSettings.cpp
    src/NetworkManager.cpp
    src/Camera.cpp
    src/DeviceManager.cpp
    src/StreamManager.cpp
    src/SystemManager.cpp
    src/cli.cpp
    src/qcustomplot.cpp 
)

set(HEADERS
    src/qcustomplot.h
)

# Create the executable
add_executable(globalSettings ${SOURCES})
add_executable(streamManager ${SOURCES})
add_executable(deviceManager ${SOURCES})
add_executable(networkManager ${SOURCES})
add_executable(systemManager ${SOURCES})
add_executable(cli ${SOURCES})

# Define the symbol to disable automatic import library linking.
target_compile_definitions(globalSettings PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(streamManager PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(deviceManager PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(networkManager PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(systemManager PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(cli PRIVATE GENICAM_NO_AUTO_IMPLIB)

# Link the executable with the rc_genicam_api library.
# The installed shared library is located at /usr/lib/x86_64-linux-gnu/librc_genicam_api.so.
foreach(target globalSettings streamManager deviceManager networkManager systemManager cli)
    target_compile_definitions(${target} PRIVATE GENICAM_NO_AUTO_IMPLIB)
    target_link_libraries(${target}
        PRIVATE
            rc_genicam_api
            ${OpenCV_LIBS}
            Qt5::Widgets
            Qt5::Core
            Qt5::Gui
            Qt5::PrintSupport
    )
endforeach()
