#include "ProxySession.h"

#include "RtspParser/RtspParser.h"


ProxySession::ProxySession(
    const std::string& clientName,
    const std::string& authToken,
    const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    ServerSession(createPeer, sendRequest, sendResponse),
    _clientName(clientName), _authToken(authToken)
{
}

bool ProxySession::onConnected() noexcept
{
    const std::string parameters =
        "name: " + _clientName + "\r\n"
        "token: " + _authToken + "\r\n";

    _authCSeq = requestSetParameter("*", "text/parameters", parameters);

    return true;
}

bool ProxySession::onGetParameterResponse(
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

bool ProxySession::onSetParameterResponse(
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

    _iceServerCSeq = requestGetParameter("*", "text/parameters", parameters);

    return true;
}
