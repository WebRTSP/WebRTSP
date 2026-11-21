cmake_minimum_required(VERSION 3.10)

project(WebRTSPPlayer)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake
)

add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_link_libraries(${PROJECT_NAME}
    Signalling
    RtStreaming
)
