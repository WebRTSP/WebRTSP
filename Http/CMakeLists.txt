cmake_minimum_required(VERSION 3.22)

project(Http)

if(WIN32)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
        cmake_policy(SET CMP0135 NEW)
    endif()

    include(FetchContent)

    FetchContent_Declare(
        microhttpd
        URL https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-latest.tar.gz
    )
    FetchContent_MakeAvailable(microhttpd)
else()
	find_package(PkgConfig REQUIRED)
	pkg_search_module(GLIB REQUIRED glib-2.0)
	pkg_search_module(SPDLOG REQUIRED spdlog)
	pkg_search_module(MICROHTTP REQUIRED libmicrohttpd)
endif()

set(SOURCES
    CMakeLists.txt
    Config.h
    Config.cpp
    Log.h
    Log.cpp
    HttpMicroServer.h
    HttpMicroServer.cpp)

add_library(${PROJECT_NAME} ${SOURCES})

if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        spdlog
        microhttpd
    )
else()
	target_include_directories(${PROJECT_NAME} PUBLIC
		${SPDLOG_INCLUDE_DIRS}
		${GLIB_INCLUDE_DIRS}
		${MICROHTTP_INCLUDE_DIRS}
	)
	target_compile_options(${PROJECT_NAME} PRIVATE ${SPDLOG_CFLAGS})
	target_link_libraries(${PROJECT_NAME}
		${SPDLOG_LDFLAGS}
		${GLIB_LDFLAGS}
		${MICROHTTP_LDFLAGS}
	)
endif()

target_link_libraries(${PROJECT_NAME}
	CxxPtr
)

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
