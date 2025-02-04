cmake_minimum_required(VERSION 3.10)
project(genicam_example)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("/usr/include/rc_genicam_api")
include_directories("/usr/include/rc_genicam_api/genicam")

# Manually add the directory containing the library.
link_directories("/usr/lib/x86_64-linux-gnu")

# Create the executable
add_executable(genicam_example src/main.cpp)

# Define the symbol to disable automatic import library linking.
target_compile_definitions(genicam_example PRIVATE GENICAM_NO_AUTO_IMPLIB)

# Link the executable with the rc_genicam_api library.
# The installed shared library is located at /usr/lib/x86_64-linux-gnu/librc_genicam_api.so.
target_link_libraries(genicam_example PRIVATE rc_genicam_api)

