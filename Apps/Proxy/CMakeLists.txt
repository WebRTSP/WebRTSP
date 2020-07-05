cmake_minimum_required(VERSION 3.0)

add_subdirectory(Proxy)

if(BUILD_PROXY)
    add_subdirectory(ProxyApp)
endif()
