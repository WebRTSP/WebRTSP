cmake_minimum_required(VERSION 3.0)

project(Http)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_search_module(SPDLOG REQUIRED spdlog)
pkg_search_module(MICROHTTP REQUIRED libmicrohttpd)

set(SOURCES
    CMakeLists.txt
    Config.h
    Config.cpp
    Log.h
    Log.cpp
    HttpMicroServer.h
    HttpMicroServer.cpp)

add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${SPDLOG_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${MICROHTTP_INCLUDE_DIRS})
target_compile_options(${PROJECT_NAME} PRIVATE ${SPDLOG_CFLAGS})
target_link_libraries(${PROJECT_NAME}
    ${SPDLOG_LDFLAGS}
    ${GLIB_LDFLAGS}
    ${MICROHTTP_LDFLAGS}
    CxxPtr)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
