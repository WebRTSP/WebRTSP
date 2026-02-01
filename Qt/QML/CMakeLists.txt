cmake_minimum_required(VERSION 3.16)

project(QmlClient LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS WebSockets REQUIRED)

qt_add_library(${PROJECT_NAME} STATIC)
qt_add_qml_module(${PROJECT_NAME}
    URI org.webrtsp.client
    VERSION 0.1
    SOURCES
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
    PLUGIN_TARGET WebRTSPClientPlugin
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Quick
        Qt6::WebSockets
        RtspSession
        RtStreaming
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
