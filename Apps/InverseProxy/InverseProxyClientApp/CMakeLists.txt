cmake_minimum_required(VERSION 3.0)

project(InverseProxyClientApp)

find_package(PkgConfig REQUIRED)

pkg_search_module(LIBCONFIG REQUIRED libconfig)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${LIBCONFIG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    ${LIBCONFIG_LIBRARIES}
    InverseProxyClient)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
