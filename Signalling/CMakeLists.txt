cmake_minimum_required(VERSION 3.22)

project(Signalling)

if(WIN32)
else()
	find_package(PkgConfig REQUIRED)
	pkg_search_module(WS REQUIRED libwebsockets)
endif()

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.cmake)

add_library(${PROJECT_NAME} ${SOURCES})

if(WIN32)
else()
	target_include_directories(${PROJECT_NAME} PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/../
		${WS_INCLUDE_DIRS}
	)
	target_link_libraries(${PROJECT_NAME}
		${WS_LDFLAGS}
	)
endif()

target_link_libraries(${PROJECT_NAME}
	RtspSession
	Helpers
	CxxPtr
)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
