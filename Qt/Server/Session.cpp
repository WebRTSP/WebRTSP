#include "Session.h"

#include "RtspParser/RtspParser.h"
#include "RtspParser/RtspSerialize.h"


using namespace webrtsp::qt;

Session::Session(
    const Config* config,
    SharedData* sharedData,
    const CreatePeer& createPeer,
    const rtsp::Session::SendRequest& sendRequest,
    const rtsp::Session::SendResponse& sendResponse) noexcept :
    ServerSession(config->webRTCConfig, createPeer, sendRequest, sendResponse),
    _config(config),
    _sharedData(sharedData)
{
}

bool Session::handleRequest(std::unique_ptr<rtsp::Request>&& requestPtr) noexcept
{
    if(_config->authToken.empty())
        return rtsp::ServerSession::handleRequest(std::move(requestPtr));

    if(_authorized) {
        if(!_authorized.value())
            return false;
    } else {
        const std::pair<rtsp::Authentication, std::string> authPair =
            rtsp::ParseAuthentication(*requestPtr);

        _authorized =
            authPair.first == rtsp::Authentication::Bearer &&
            authPair.second == _config->authToken;

        if(!_authorized.value())
            return false;
    }

    return rtsp::ServerSession::handleRequest(std::move(requestPtr));
}

bool Session::listEnabled(const std::string& uri) noexcept
{
    return uri == rtsp::WildcardUri;
}

bool Session::onListRequest(std::unique_ptr<rtsp::Request>&& requestPtr) noexcept
{
    const std::string& uri = requestPtr->uri;

    if(uri == rtsp::WildcardUri) {
        sendOkResponse(requestPtr->cseq, rtsp::TextParametersContentType, _sharedData->listCache);
        return true;
    }

    return false;
}
