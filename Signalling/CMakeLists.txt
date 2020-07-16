cmake_minimum_required(VERSION 3.0)

project(Signalling)

find_package(PkgConfig REQUIRED)
pkg_search_module(WS REQUIRED libwebsockets)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake)

add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${WS_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    ${WS_LDFLAGS}
    RtspSession
    Common)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
