#include "InverseProxyClientSession.h"


InverseProxyClientSession::InverseProxyClientSession(
    const std::string& clientName,
    const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    ServerSession(createPeer, sendRequest, sendResponse), _clientName(clientName)
{
}

bool InverseProxyClientSession::onConnected() noexcept
{
    const std::string parameters = "name:" + _clientName + "\r\n";

    requestSetParameter("*", "text/parameters", parameters);

    return true;
}
