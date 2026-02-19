cmake_minimum_required(VERSION 3.10)

project(Client)

find_package(PkgConfig REQUIRED)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake)

add_library(${PROJECT_NAME} ${SOURCES})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DGST_USE_UNSTABLE_API)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(${PROJECT_NAME}
    GstRtStreaming
    Helpers)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
