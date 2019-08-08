cmake_minimum_required(VERSION 3.0)

project(WebRTSP)

if(CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wall)
endif(CMAKE_COMPILER_IS_GNUCC)

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
add_subdirectory(Signalling)
add_subdirectory(Streaming)
add_subdirectory(Client)
add_subdirectory(Test)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
