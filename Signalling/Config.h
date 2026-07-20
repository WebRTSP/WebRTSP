#pragma once

#include <cstdint>
#include <string>


enum: uint16_t {
    WEBRTSP_DEFAULT_WS_PORT = 5554,
    WEBRTSP_DEFAULT_WSS_PORT = 5555,
};

struct WsServerConfig
{
    bool bindToLoopbackOnly = true;
    unsigned short port = WEBRTSP_DEFAULT_WS_PORT;
};

struct WsClientConfig
{
    std::string server;
    unsigned short serverPort;
    bool useTls = true;
};

bool FillConfigFromUrl(const char*, WsClientConfig*);
inline bool FillConfigFromUrl(const std::string& url, WsClientConfig* config)
    { return FillConfigFromUrl(url.c_str(), config); }
