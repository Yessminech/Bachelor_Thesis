# Add this to your gui/CMakeLists.txt
include_directories(${CMAKE_SOURCE_DIR}/../src) 
include_directories(${CMAKE_SOURCE_DIR}/../)    
include_directories("/usr/include/rc_genicam_api")
include_directories("/usr/include/rc_genicam_api/genicam")
cmake_minimum_required(VERSION 3.10)
project(QtOpenCVDemo)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
find_package(OpenCV REQUIRED)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/qcustomplot.cpp 
    src/CameraSettingsWindow.cpp   
    ${CMAKE_SOURCE_DIR}/../src/DeviceManager.cpp
    ${CMAKE_SOURCE_DIR}/../src/StreamManager.cpp
    ${CMAKE_SOURCE_DIR}/../src/NetworkManager.cpp
    ${CMAKE_SOURCE_DIR}/../src/Camera.cpp
    ${CMAKE_SOURCE_DIR}/../src/GlobalSettings.cpp
)

set(HEADERS
    src/MainWindow.hpp
    src/qcustomplot.h 
    src/CameraSettingsWindow.hpp
)

add_executable(QtOpenCVDemo ${SOURCES} ${HEADERS})
target_link_libraries(QtOpenCVDemo PRIVATE 
    Qt6::Widgets 
    Qt6::PrintSupport 
    ${OpenCV_LIBS}
    rc_genicam_api
)