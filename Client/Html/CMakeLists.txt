cmake_minimum_required(VERSION 3.0)

project(HtmlClient)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.html
    *.js
    *.cmake)

add_custom_target(${PROJECT_NAME} SOURCES ${SOURCES})

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
