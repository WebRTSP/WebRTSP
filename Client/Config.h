#pragma once

#include <string>


namespace client {

struct Config
{
    std::string server;
    unsigned short serverPort;
    bool useTls = true;
};

bool FillConfigFromUrl(const char*, Config*);
inline bool FillConfigFromUrl(const std::string& url, Config* config)
    { return FillConfigFromUrl(url.c_str(), config); }

}
