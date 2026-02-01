#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <chrono>
#include <filesystem>

#include <spdlog/common.h>

#include "Client/Config.h"
#include "Signalling/Config.h"

#include "RtStreaming/WebRTCConfig.h"


namespace webrtsp::qt {

struct StreamerConfig
{
    enum class Type {
        ReStreamer,
    };

    Type type;
    std::string uri;
    std::string description;
    std::string forceH264ProfileLevelId;
};


struct Config : public signalling::Config
{
    std::shared_ptr<WebRTCConfig> webRTCConfig = std::make_shared<WebRTCConfig>();
    std::map<std::string, StreamerConfig> streamers; // escaped streamer name -> StreamerConfig
    std::string authToken;
};

}
