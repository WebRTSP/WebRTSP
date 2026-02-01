#pragma once

#include <map>

#include "RtspSession/ServerSession.h"
#include "RtStreaming/GstRtStreaming/GstStreamingSource.h"

#include "Config.h"


namespace webrtsp::qt {

class Session : public rtsp::ServerSession
{
public:
    typedef std::map<std::string, std::unique_ptr<GstStreamingSource>> Streamers;

    struct SharedData {
        const std::string listCache;
        Streamers streamers;
    };

    Session(
        const Config*,
        SharedData*,
        const CreatePeer& createPeer,
        const rtsp::Session::SendRequest& sendRequest,
        const rtsp::Session::SendResponse& sendResponse) noexcept;

    bool handleRequest(std::unique_ptr<rtsp::Request>&&) noexcept override;

protected:
    bool listEnabled(const std::string& uri) noexcept override;

    bool onListRequest(std::unique_ptr<rtsp::Request>&&) noexcept override;

private:
    const Config *const _config;
    SharedData *const _sharedData;
    std::optional<bool> _authorized;
};

}
