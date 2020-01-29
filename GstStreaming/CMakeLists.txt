cmake_minimum_required(VERSION 3.0)

project(GstStreaming)

find_package(PkgConfig REQUIRED)
pkg_search_module(GSTREAMER REQUIRED gstreamer-1.0)
pkg_search_module(GSTREAMER_SDP REQUIRED gstreamer-sdp-1.0)
pkg_search_module(GSTREAMER_WEBRTC REQUIRED gstreamer-webrtc-1.0)
pkg_search_module(GSTREAMER_PBUTILS REQUIRED gstreamer-pbutils-1.0)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake)

add_library(${PROJECT_NAME} ${SOURCES})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DGST_USE_UNSTABLE_API)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_SDP_INCLUDE_DIRS}
    ${GSTREAMER_WEBRTC_INCLUDE_DIRS}
    ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
    ${WS_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    ${GSTREAMER_LDFLAGS}
    ${GSTREAMER_SDP_LDFLAGS}
    ${GSTREAMER_WEBRTC_LDFLAGS}
    ${GSTREAMER_PBUTILS_LDFLAGS}
    RtspSession
    Common)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
