cmake_minimum_required(VERSION 3.10)

project(Signalling)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBGIO REQUIRED gio-2.0)

find_package(OpenSSL REQUIRED)

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

if(ANDROID)
    target_link_libraries(${PROJECT_NAME}
        PUBLIC
            ${OPENSSL_STATIC_LIBRARIES}
    )
else()
    target_link_libraries(${PROJECT_NAME}
        ${OPENSSL_LIBRARIES}
    )
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${LIBGIO_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../
)

target_link_libraries(${PROJECT_NAME}
    ${LIBGIO_LDFLAGS}
    RtspSession
    Helpers
    CxxPtr
)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
