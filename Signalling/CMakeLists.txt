cmake_minimum_required(VERSION 3.10)

project(Signalling)

if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
endif()

if(NOT ANDROID AND NOT WIN32)
    pkg_search_module(LIBWEBSOCKETS REQUIRED libwebsockets)
endif()

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake
)

add_library(${PROJECT_NAME} ${SOURCES})

if(ANDROID OR WIN32)
    target_link_libraries(${PROJECT_NAME}
        websockets
    )
else()
    target_include_directories(${PROJECT_NAME}
        PUBLIC
            ${LIBWEBSOCKETS_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}
        ${LIBWEBSOCKETS_LDFLAGS}
    )
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../
)

target_link_libraries(${PROJECT_NAME}
    RtspSession
    Helpers
    CxxPtr
)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
