cmake_minimum_required(VERSION 3.0)

project(Http)

option(USE_LIBMICROHTTPD "enable libmicrohttpd based HTTP server" ON)

find_package(PkgConfig REQUIRED)
pkg_search_module(WS REQUIRED libwebsockets)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_search_module(SPDLOG REQUIRED spdlog)

if(USE_LIBMICROHTTPD)
pkg_search_module(MICROHTTP REQUIRED libmicrohttpd)
endif()

set(SOURCES
    CMakeLists.txt
    Config.h
    Config.cpp
    Log.h
    Log.cpp
    HttpServer.h
    HttpServer.cpp)

if(USE_LIBMICROHTTPD)
set(LIBMICROHTTPD_SOURCES
    HttpMicroServer.h
    HttpMicroServer.cpp)
endif()

add_library(${PROJECT_NAME} ${SOURCES} ${LIBMICROHTTPD_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${WS_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS})
target_compile_options(${PROJECT_NAME} PRIVATE ${SPDLOG_CFLAGS})
target_link_libraries(${PROJECT_NAME}
    ${WS_LDFLAGS}
    ${SPDLOG_LDFLAGS}
    ${GLIB_LDFLAGS}
    CxxPtr)

if(USE_LIBMICROHTTPD)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${MICROHTTP_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    ${MICROHTTP_LDFLAGS})
endif()

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
