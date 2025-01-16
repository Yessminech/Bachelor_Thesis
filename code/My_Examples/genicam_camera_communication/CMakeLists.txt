cmake_minimum_required(VERSION 3.16) #for Qt # TODO: Ask Olli
project(babib)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include the GenTL headers
include_directories(${CMAKE_SOURCE_DIR}/genicam/library/CPP/include)

# Add the executable
add_executable(list_devices src/list_devices.cc)

# Link the GenTL libraries
# TODO: Adapt this to more architectures
set(GENICAM_LIBRARIES_DIR ${CMAKE_SOURCE_DIR}/genicam/bin/Linux64_x64) 
set(GENICAM_LIB_SUFFIX ".so")

set(GENICAM_LIBRARIES
    libGCBase_gcc48_v3_4
    libGenApi_gcc48_v3_4
    liblog4cpp_gcc48_v3_4
    libLog_gcc48_v3_4
    libMathParser_gcc48_v3_4
    libNodeMapData_gcc48_v3_4
    libXmlParser_gcc48_v3_4
)

foreach(GENICAM_LIB ${GENICAM_LIBRARIES})
    target_link_libraries(list_devices ${GENICAM_LIBRARIES_DIR}/${GENICAM_LIB}${GENICAM_LIB_SUFFIX})
endforeach()
