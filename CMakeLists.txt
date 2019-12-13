cmake_minimum_required(VERSION 3.0)

project(WebRTSP)

option(BUILD_TEST "Build test application" ON)
option(BUILD_BASIC_SERVER "Build basic server application" ON)

if(CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wall)
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
add_subdirectory(Signalling)
add_subdirectory(Streaming)
add_subdirectory(Client)

if(BUILD_TEST)
    add_subdirectory(Test)
endif()

if(BUILD_BASIC_SERVER)
    add_subdirectory(Apps/BasicServer)
endif()

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
