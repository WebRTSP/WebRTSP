cmake_minimum_required(VERSION 3.0)

project(WebRTSP)

option(BUILD_TEST_APPS "Build test applications" OFF)
option(BUILD_BASIC_SERVER "Build basic server application" OFF)

if(DEFINED ENV{SNAPCRAFT_BUILD_ENVIRONMENT})
    add_definitions(-DSNAPCRAFT_BUILD=1)
    set(SNAPCRAFT_BUILD YES)
endif()

if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_CXX_FLAGS_DEBUG "-Wall -g -O0")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${DEPS_DIR})

add_subdirectory(deps/CxxPtr)
add_subdirectory(Common)

add_subdirectory(RtspParser)
add_subdirectory(RtspSession)
add_subdirectory(GstStreaming)
add_subdirectory(Signalling)
add_subdirectory(Client)
add_subdirectory(Http)

if(BUILD_TEST_APPS)
    add_subdirectory(Apps/Test)
endif()

if(BUILD_BASIC_SERVER)
    add_subdirectory(Apps/BasicServer)
endif()

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
