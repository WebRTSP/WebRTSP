#pragma once

#include "RtStreaming/WebRTCConfig.h"


namespace rtsp {

struct WebRTCSessionMixin
{
    virtual const WebRTCConfigPtr& webRTCConfig() const { return _webRTCConfig; }

protected:
    WebRTCSessionMixin(const WebRTCConfigPtr& webRTCConfig) noexcept :
        _webRTCConfig(webRTCConfig)
    {}

    void setWebRTCConfig(WebRTCConfigPtr&& webRTCConfig) noexcept
        { _webRTCConfig = std::move(webRTCConfig); }

private:
    WebRTCConfigPtr _webRTCConfig;
};

}
