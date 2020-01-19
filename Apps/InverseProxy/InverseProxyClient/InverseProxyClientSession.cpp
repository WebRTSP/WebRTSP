#include "InverseProxyClientSession.h"

#include "RtspParser/RtspParser.h"


InverseProxyClientSession::InverseProxyClientSession(
    const std::string& clientName,
    const std::string& authToken,
    const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    ServerSession(createPeer, sendRequest, sendResponse),
    _clientName(clientName), _authToken(authToken)
{
}

bool InverseProxyClientSession::onConnected() noexcept
{
    const std::string parameters =
        "name: " + _clientName + "\r\n"
        "token: " + _authToken + "\r\n";

    _authCSeq = requestSetParameter("*", "text/parameters", parameters);

    return true;
}

bool InverseProxyClientSession::onGetParameterResponse(
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

    auto turnServerIt = parameters.find("turn-server");
    if(parameters.end() != turnServerIt)
        setIceServers({"turn://" + turnServerIt->second});
    else
        setIceServers({});

    return true;
}

bool InverseProxyClientSession::onSetParameterResponse(
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
