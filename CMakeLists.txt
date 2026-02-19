cmake_minimum_required(VERSION 3.10)

project(WebRTSP)

option(BUILD_TEST_APPS "Build test applications" OFF)
option(BUILD_BASIC_SERVER "Build basic server application" OFF)
option(HTTP_SUPPORT "HTTP server support" ON)
option(WS_SERVER_SUPPORT "libwebsockets based server implementation" ON)
option(WS_CLIENT_SUPPORT "libwebsockets based client implementation" ON)
option(QML_SUPPORT "QML support" OFF)
option(QT_SERVER_SUPPORT "Qt QWebSocketServer based server implementation" OFF)

if(NOT WS_SERVER_SUPPORT AND NOT WS_CLIENT_SUPPORT)
    set(LIBWEBSOCKETS_SUPPORT OFF CACHE BOOL "Disable all references to libwebsockets" FORCE)
endif()

if(DEFINED ENV{SNAPCRAFT_BUILD_ENVIRONMENT})
    add_definitions(-DSNAPCRAFT_BUILD=1)
    set(SNAPCRAFT_BUILD YES)
endif()

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(ANDROID OR WIN32)
    include(FetchContent)

    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.17.0
    )
    FetchContent_MakeAvailable(spdlog)

    if(HTTP_SUPPORT)
        FetchContent_Declare(
          microhttpd
          GIT_REPOSITORY https://git.gnunet.org/libmicrohttpd.git
          GIT_TAG v1.0.1
        )

        FetchContent_MakeAvailable(microhttpd)
    endif()

    if(LIBWEBSOCKETS_SUPPORT)
        FetchContent_Declare(
            libwebsockets
            GIT_REPOSITORY https://github.com/warmcat/libwebsockets.git
            GIT_TAG v4.5-stable
        )
        FetchContent_MakeAvailable(libwebsockets)
    endif()
endif()

if(ANDROID)
    if(NOT DEFINED ENV{GSTREAMER_ANDROID_ROOT})
       message(FATAL_ERROR "GSTREAMER_ANDROID_ROOT environment variable is not defined")
    endif()

    set(GSTREAMER_ANDROID_ROOT $ENV{GSTREAMER_ANDROID_ROOT})

    if(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "x86" OR ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "x86_64")
        set(GSTREAMER_ANDROID_ARCH ${CMAKE_ANDROID_ARCH_ABI})
    elseif(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a")
        set(GSTREAMER_ANDROID_ARCH arm64)
    elseif(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "armeabi-v7a")
        set(GSTREAMER_ANDROID_ARCH armv7)
    endif()

    set(GSTREAMER_ANDROID_ROOT ${GSTREAMER_ANDROID_ROOT}/${GSTREAMER_ANDROID_ARCH})

    set(GSTREAMER_PKG_PATHS
        "${GSTREAMER_ANDROID_ROOT}/lib/pkgconfig"
        "${GSTREAMER_ANDROID_ROOT}/lib/gstreamer-1.0/pkgconfig"
    )

    cmake_path(CONVERT "${GSTREAMER_PKG_PATHS}" TO_NATIVE_PATH_LIST GSTREAMER_PKG_PATHS)

    set(ENV{PKG_CONFIG_PATH} "${GSTREAMER_PKG_PATHS}")
endif()

add_subdirectory(Helpers)
add_subdirectory(RtspParser)
add_subdirectory(RtspSession)
add_subdirectory(RtStreaming)
if(WS_SERVER_SUPPORT)
    add_subdirectory(Signalling)
endif()
if(WS_CLIENT_SUPPORT)
    add_subdirectory(Client)
endif()
if(HTTP_SUPPORT)
    add_subdirectory(Http)
endif()
if(QML_SUPPORT)
    add_subdirectory(Qt/QML)
endif()
if(QT_SERVER_SUPPORT)
    add_subdirectory(Qt/Server)
endif()

if(BUILD_TEST_APPS)
    add_subdirectory(Apps/Test)
    add_subdirectory(Apps/RecordTest)
endif()

if(BUILD_BASIC_SERVER)
    add_subdirectory(Apps/BasicServer)
endif()

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
