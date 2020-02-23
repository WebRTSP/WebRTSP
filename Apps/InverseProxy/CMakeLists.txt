cmake_minimum_required(VERSION 3.0)

add_subdirectory(InverseProxyServer)
add_subdirectory(InverseProxyAgent)

if(BUILD_TEST_APPS)
    add_subdirectory(InverseProxyTest)
endif()

if(BUILD_INVERSE_PROXY_SERVER)
    add_subdirectory(InverseProxyServerApp)
endif()

if(BUILD_INVERSE_PROXY_AGENT)
    add_subdirectory(InverseProxyAgentApp)
endif()
