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
    unsigned short serverPort = WEBRTSP_DEFAULT_WSS_PORT;
    bool useTls = true;
};

struct WsClientCredentials
{
    std::string agentId;
    std::string accessToken;
};

bool WebRTSPUrlParse(
    const char*,
    WsClientConfig*,
    WsClientCredentials* = nullptr,
    std::string* outPath = nullptr);
inline bool WebRTSPUrlParse(
    const std::string& url,
    WsClientConfig* config,
    WsClientCredentials* outCredentials = nullptr,
    std::string* outPath = nullptr)
{ return WebRTSPUrlParse(url.c_str(), config, outCredentials, outPath); }
