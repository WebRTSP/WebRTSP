cmake_minimum_required(VERSION 3.0)

project(Test)

find_package(Threads REQUIRED)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake)

add_executable(${PROJECT_NAME} ${SOURCES})
#target_include_directories(${PROJECT_NAME} PRIVATE
#    )
target_link_libraries(${PROJECT_NAME}
    RtspParser
    Signalling
    GstStreaming
    Client
    Threads::Threads)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
