cmake_minimum_required(VERSION 3.19)

project(QtServer LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS WebSockets)

add_library(${PROJECT_NAME} STATIC
    Config.h
    Server.h
    Server.cpp
    Session.h
    Session.cpp
    ../QActor.h
    ../QActor.cpp
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt6::Core
        Qt6::WebSockets
        RtspSession
        RtStreaming
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
