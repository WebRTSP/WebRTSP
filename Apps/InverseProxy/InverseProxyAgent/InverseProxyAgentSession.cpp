#include "InverseProxyAgentSession.h"

#include <glib.h>

#include <CxxPtr/CPtr.h>

#include "RtspParser/RtspParser.h"

#include "Config.h"


InverseProxyAgentSession::InverseProxyAgentSession(
    const InverseProxyAgentConfig* config,
    Cache* cache,
    const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    ServerSession(createPeer, sendRequest, sendResponse),
    _config(config), _cache(cache)
{
}

bool InverseProxyAgentSession::onConnected() noexcept
{
    if(_cache->parameters.empty()) {
        _cache->parameters =
            "name: " + _config->name + "\r\n"
            "token: " + _config->authToken + "\r\n";
        if(!_config->description.empty())
            _cache->parameters +=
                "description: " + _config->description + "\r\n";
    }

    _authCSeq = requestSetParameter("*", "text/parameters", _cache->parameters);

    return true;
}

bool InverseProxyAgentSession::onOptionsRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    rtsp::Response response;
    prepareOkResponse(requestPtr->cseq, &response);

    response.headerFields.emplace("Public", "LIST, DESCRIBE, SETUP, PLAY, TEARDOWN");

    sendResponse(response);

    return true;
}

bool InverseProxyAgentSession::onListRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    if(_cache->list.empty()) {
        if(_config->streamers.empty())
            _cache->list = "\r\n";
        else {
            for(const auto& pair: _config->streamers) {
                CharPtr escapedNamePtr(
                    g_uri_escape_string(pair.first.data(), nullptr, false));
                if(!escapedNamePtr)
                    return false; // insufficient memory?

                _cache->list += escapedNamePtr.get();
                _cache->list += ": ";
                _cache->list += pair.second.description;
                _cache->list += + "\r\n";
            }
        }
    }

    sendOkResponse(requestPtr->cseq, "text/parameters", _cache->list);

    return true;
}

bool InverseProxyAgentSession::onGetParameterResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(!ServerSession::onGetParameterResponse(request, response))
        return false;

    if(_iceServerCSeq && response.cseq != _iceServerCSeq)
        return false;

    _iceServerCSeq = 0;

    rtsp::Parameters parameters;
    if(!rtsp::ParseParameters(response.body, &parameters))
        return false;

    WebRTCPeer::IceServers iceServers;

    auto stunServerIt = parameters.find("stun-server");
    if(parameters.end() != stunServerIt && !stunServerIt->second.empty())
        iceServers.push_back({"stun://" + stunServerIt->second});

    auto turnServerIt = parameters.find("turn-server");
    if(parameters.end() != turnServerIt && !turnServerIt->second.empty())
        iceServers.push_back({"turn://" + turnServerIt->second});

    auto turnsServerIt = parameters.find("turns-server");
    if(parameters.end() != turnsServerIt && !turnsServerIt->second.empty())
        iceServers.push_back({"turns://" + turnsServerIt->second});

    setIceServers(iceServers);

    return true;
}

bool InverseProxyAgentSession::onSetParameterResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(!ServerSession::onSetParameterResponse(request, response))
        return false;

    if(_authCSeq && response.cseq != _authCSeq)
        return false;

    _authCSeq = 0;

    const std::string parameters =
        "ice-servers\r\n";

    _iceServerCSeq = requestGetParameter("*", "text/list", parameters);

    return true;
}
