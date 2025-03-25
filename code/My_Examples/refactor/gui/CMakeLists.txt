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
)

set(HEADERS
    src/MainWindow.hpp
    src/qcustomplot.h 
    src/CameraSettingsWindow.hpp
)

file(COPY icons/play.png icons/stop.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/icons)

add_executable(QtOpenCVDemo ${SOURCES} ${HEADERS})
target_link_libraries(QtOpenCVDemo PRIVATE Qt6::Widgets Qt6::PrintSupport ${OpenCV_LIBS})
