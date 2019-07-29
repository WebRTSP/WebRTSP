cmake_minimum_required(VERSION 3.0)

project(RtspParser)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake)

add_library(${PROJECT_NAME} ${SOURCES})
#target_include_directories(${PROJECT_NAME} PRIVATE
#    )
#target_link_libraries(${PROJECT_NAME}
#    )

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
