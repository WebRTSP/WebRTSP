cmake_minimum_required(VERSION 3.10)

project(Http)

if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
endif()

if(NOT ANDROID AND NOT WIN32)
    pkg_search_module(SPDLOG REQUIRED spdlog)
endif()

if(WIN32)
    set(GSTREAMER_ROOT_DIR "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}")
    set(GLIB_INCLUDE_DIRS
        "${GSTREAMER_ROOT_DIR}/include/glib-2.0"
        "${GSTREAMER_ROOT_DIR}/lib/glib-2.0/include"
    )
    set(GLIB_LDFLAGS
        -l"${GSTREAMER_ROOT_DIR}/lib/glib-2.0.lib"
    )
else()
    pkg_search_module(GLIB REQUIRED glib-2.0)
endif()

if(NOT WIN32)
    pkg_search_module(MICROHTTP REQUIRED libmicrohttpd)
endif()

set(SOURCES
    CMakeLists.txt
    Config.h
    Config.cpp
    Log.h
    Log.cpp
    HttpMicroServer.h
    HttpMicroServer.cpp
)

add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${GLIB_INCLUDE_DIRS}
    ${MICROHTTP_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME}
    ${GLIB_LDFLAGS}
    ${MICROHTTP_LDFLAGS}
    CxxPtr
)

if(ANDROID OR WIN32)
    target_link_libraries(${PROJECT_NAME}
        spdlog::spdlog
    )
else()
    target_include_directories(${PROJECT_NAME}
        PUBLIC
            ${SPDLOG_INCLUDE_DIRS}
    )
    target_compile_options(${PROJECT_NAME}
        PUBLIC
            ${SPDLOG_CFLAGS}
    )
    target_link_libraries(${PROJECT_NAME}
        ${SPDLOG_LDFLAGS}
    )
endif()

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
