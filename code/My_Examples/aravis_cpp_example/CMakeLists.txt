cmake_minimum_required(VERSION 3.10)
project(aravis_cpp_example VERSION 1.0 LANGUAGES CXX)

# Find the Aravis package
find_package(PkgConfig REQUIRED)
pkg_check_modules(ARAVIS REQUIRED aravis-0.8)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)

# Include Aravis headers
include_directories(${ARAVIS_INCLUDE_DIRS})

# Add the executable
add_executable(aravis_cpp_example src/04-camera-features.cpp)

# Link against Aravis
target_link_libraries(aravis_cpp_example ${ARAVIS_LIBRARIES})
