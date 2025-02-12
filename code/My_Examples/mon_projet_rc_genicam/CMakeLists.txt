cmake_minimum_required(VERSION 3.10)
project(genicam_example)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

include_directories("/usr/include/rc_genicam_api")
include_directories("/usr/include/rc_genicam_api/genicam")
include_directories(${OpenCV_INCLUDE_DIRS})

# Manually add the directory containing the library.
link_directories("/usr/lib/x86_64-linux-gnu")

# Create the executable
add_executable(main_example src/main.cpp)
add_executable(set_feature src/set_feature_from_terminal.cpp)
add_executable(get_timestamp src/get_timestamp.cpp)
add_executable(ptp_with_timestamps src/ptp_with_timestamps.cpp)
add_executable(stream src/stream.cpp)
add_executable(sched_ac src/sched_ac.cpp)


# Define the symbol to disable automatic import library linking.
target_compile_definitions(main_example PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(set_feature PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(get_timestamp PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(ptp_with_timestamps PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(stream PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(sched_ac PRIVATE GENICAM_NO_AUTO_IMPLIB)

# Link the executable with the rc_genicam_api library.
# The installed shared library is located at /usr/lib/x86_64-linux-gnu/librc_genicam_api.so.
target_link_libraries(main_example PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(set_feature PRIVATE rc_genicam_api )
target_link_libraries(get_timestamp PRIVATE rc_genicam_api )
target_link_libraries(ptp_with_timestamps PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(stream PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(sched_ac PRIVATE rc_genicam_api ${OpenCV_LIBS})
