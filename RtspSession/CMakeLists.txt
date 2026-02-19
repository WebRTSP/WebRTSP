cmake_minimum_required(VERSION 3.10)

project(RtspSession)

if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
endif()

if(NOT ANDROID AND NOT WIN32)
    pkg_search_module(SPDLOG REQUIRED spdlog)
endif()

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake)

add_library(${PROJECT_NAME} ${SOURCES})

if(ANDROID OR WIN32)
    target_link_libraries(${PROJECT_NAME}
        spdlog::spdlog
    )
else()
    target_include_directories(${PROJECT_NAME}
        PUBLIC
            ${SPDLOG_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}
        ${SPDLOG_LDFLAGS}
    )
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../
)

target_link_libraries(${PROJECT_NAME}
    RtspParser
    Helpers
)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
