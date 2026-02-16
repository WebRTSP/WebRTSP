cmake_minimum_required(VERSION 3.16)

project(QmlClient LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)
find_package(Qt6 REQUIRED COMPONENTS WebSockets)

if(ANDROID)
    pkg_check_modules(GSTREAMER_PLUGINS
        REQUIRED
            gstopengl
            gstqml6
    )
endif()

qt_add_library(${PROJECT_NAME} STATIC)
qt_add_qml_module(${PROJECT_NAME}
    URI org.webrtsp.client
    VERSION 0.1
    SOURCES
        QmlLibGst.h
        QmlLibGst.cpp
        Connection.h
        Connection.cpp
        ConnectionClient.h
        ConnectionClient.cpp
        UriInfo.h
        UriInfo.cpp
        Player.h
        Player.cpp
        Peer.h
        Peer.cpp
        ../QActor.h
        ../QActor.cpp
    PLUGIN_TARGET WebRTSPClientPlugin
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt6::Quick
        Qt6::WebSockets
        RtspSession
        RtStreaming
)

if(ANDROID)
    target_link_libraries(${PROJECT_NAME}
        PUBLIC
            ${GSTREAMER_PLUGINS_STATIC_LDFLAGS}
    )
endif()
