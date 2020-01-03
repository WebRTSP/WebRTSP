#include "InverseProxyClientSession.h"


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

    requestSetParameter("*", "text/parameters", parameters);

    return true;
}
