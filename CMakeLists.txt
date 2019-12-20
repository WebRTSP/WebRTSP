cmake_minimum_required(VERSION 3.0)

project(WebRTSP)

option(BUILD_TEST "Build test application" ON)
option(BUILD_BASIC_SERVER "Build basic server application" ON)
option(BUILD_INVERSE_PROXY "Build inverse proxy application" ON)

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

if(BUILD_TEST)
    add_subdirectory(Apps/Test)
endif()

if(BUILD_BASIC_SERVER)
    add_subdirectory(Apps/BasicServer)
endif()

if(BUILD_INVERSE_PROXY)
    add_subdirectory(Apps/InverseProxy)
endif()

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
