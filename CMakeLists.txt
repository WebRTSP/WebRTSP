cmake_minimum_required(VERSION 3.10)

project(WebRTSP)

option(BUILD_TESTS_APP "Build tests application" OFF)
option(HTTP_SUPPORT "HTTP server support" ON)
option(SIGNALLING_SUPPORT "libwebsockets based server and client implementations" ON)
option(STREAMING_SUPPORT "Streaming implementation support" ON)
option(QML_SUPPORT "QML support" OFF)
option(QT_SERVER_SUPPORT "Qt QWebSocketServer based server implementation" OFF)

if(SIGNALLING_SUPPORT)
    set(LIBWEBSOCKETS_SUPPORT ON CACHE BOOL "libwebsockets enabled" FORCE)
else()
    set(LIBWEBSOCKETS_SUPPORT OFF CACHE BOOL "Disable all references to libwebsockets" FORCE)
endif()

if(DEFINED ENV{SNAPCRAFT_BUILD_ENVIRONMENT})
    add_definitions(-DSNAPCRAFT_BUILD=1)
    set(SNAPCRAFT_BUILD YES)
endif()

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
endif()

if(ANDROID)
    if(NOT DEFINED GSTREAMER_ANDROID_ROOT)
        if(NOT DEFINED ENV{GSTREAMER_ANDROID_ROOT})
           message(FATAL_ERROR "GSTREAMER_ANDROID_ROOT environment variable is not defined")
        endif()

        set(GSTREAMER_ANDROID_ROOT $ENV{GSTREAMER_ANDROID_ROOT})
    endif()

    if(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "x86" OR ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "x86_64")
        set(GSTREAMER_ANDROID_ARCH ${CMAKE_ANDROID_ARCH_ABI})
    elseif(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a")
        set(GSTREAMER_ANDROID_ARCH arm64)
    elseif(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "armeabi-v7a")
        set(GSTREAMER_ANDROID_ARCH armv7)
    endif()

    set(GSTREAMER_ANDROID_ARCH_ROOT ${GSTREAMER_ANDROID_ROOT}/${GSTREAMER_ANDROID_ARCH})

    cmake_path(CONVERT "$ENV{PKG_CONFIG_PATH}" TO_NATIVE_PATH_LIST PKG_CONFIG_PATH NORMALIZE)

    list(APPEND PKG_CONFIG_PATH
        "${GSTREAMER_ANDROID_ARCH_ROOT}/lib/pkgconfig"
        "${GSTREAMER_ANDROID_ARCH_ROOT}/lib/gstreamer-1.0/pkgconfig"
    )

    list(REMOVE_DUPLICATES PKG_CONFIG_PATH)

    cmake_path(CONVERT "${PKG_CONFIG_PATH}" TO_NATIVE_PATH_LIST PKG_CONFIG_PATH)

    set(ENV{PKG_CONFIG_PATH} "${PKG_CONFIG_PATH}")

    # GStreamer has some libraries (like OpenSSL) embedded
    list(APPEND CMAKE_PREFIX_PATH "${GSTREAMER_ANDROID_ARCH_ROOT}")
    list(APPEND CMAKE_FIND_ROOT_PATH "${GSTREAMER_ANDROID_ARCH_ROOT}")
endif()

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
        if(ANDROID)
            set(OPENSSL_USE_STATIC_LIBS YES)
            # it's required to prevent use of OpenSSL build from KDAB in ONVIF
            find_package(OpenSSL REQUIRED)

            set(LWS_WITH_STATIC ON)
            set(LWS_WITH_SHARED OFF)
        endif()

        set(LWS_WITH_MINIMAL_EXAMPLES OFF)
        set(LWS_WITHOUT_TESTAPPS ON)
        set(LWS_WITHOUT_TEST_SERVER ON)
        set(LWS_WITHOUT_TEST_SERVER_EXTPOLL ON)
        set(LWS_WITHOUT_TEST_PING ON)
        set(LWS_WITHOUT_TEST_CLIENT ON)
        set(LWS_WITHOUT_DAEMONIZE ON)
        set(LWS_WITH_GLIB ON)

        FetchContent_Declare(
            libwebsockets
            GIT_REPOSITORY https://github.com/warmcat/libwebsockets.git
            GIT_TAG v4.5-stable
        )
        FetchContent_MakeAvailable(libwebsockets)
    endif()
else()
    pkg_search_module(LIBWEBSOCKETS REQUIRED libwebsockets)
endif()

add_subdirectory(Helpers)
add_subdirectory(RtspParser)
add_subdirectory(RtspSession)
if(STREAMING_SUPPORT)
    add_subdirectory(RtStreaming)
else()
    add_subdirectory(RtStreaming/deps/CxxPtr)
endif()
if(SIGNALLING_SUPPORT)
    add_subdirectory(Signalling)
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

if(BUILD_TESTS_APP)
    add_subdirectory(Tests)
endif()

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
