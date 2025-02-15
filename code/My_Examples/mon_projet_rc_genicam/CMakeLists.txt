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
add_executable(main_example src/main.cpp)
add_executable(cam_ctrl src/cam_ctrl.cpp)
add_executable(ptp_ctrl src/ptp_ctrl.cpp)
add_executable(continuous_stream src/continuous_stream.cpp)
add_executable(single_shot src/single_shot.cpp)
add_executable(sched_ac src/sched_ac.cpp)
add_executable(list_all src/list_all.cpp)


# Define the symbol to disable automatic import library linking.
target_compile_definitions(main_example PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(cam_ctrl PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(ptp_ctrl PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(continuous_stream PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(single_shot PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(sched_ac PRIVATE GENICAM_NO_AUTO_IMPLIB)
target_compile_definitions(list_all PRIVATE GENICAM_NO_AUTO_IMPLIB)

# Link the executable with the rc_genicam_api library.
# The installed shared library is located at /usr/lib/x86_64-linux-gnu/librc_genicam_api.so.
target_link_libraries(main_example PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(cam_ctrl PRIVATE rc_genicam_api )
target_link_libraries(ptp_ctrl PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(continuous_stream PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(single_shot PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(sched_ac PRIVATE rc_genicam_api ${OpenCV_LIBS})
target_link_libraries(list_all PRIVATE rc_genicam_api ${OpenCV_LIBS})