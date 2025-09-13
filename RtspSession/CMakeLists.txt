cmake_minimum_required(VERSION 3.10)

project(RtspSession)

find_package(PkgConfig REQUIRED)
pkg_search_module(SPDLOG REQUIRED spdlog)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake)

add_library(${PROJECT_NAME} ${SOURCES})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${SPDLOG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    RtspParser
    Helpers
    ${SPDLOG_LDFLAGS})

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
