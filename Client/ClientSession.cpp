#include "ClientSession.h"

#include "RtspSession/StatusCode.h"

namespace {

rtsp::SessionId ResponseSession(const rtsp::Response& response)
{
    auto it = response.headerFields.find("session");
    if(response.headerFields.end() == it)
        return rtsp::SessionId();

    return it->second;
}

}


struct ClientSession::Private
{
    ClientSession* owner;

    GstClient gstClient;
    std::string remoteSdp;
    rtsp::SessionId session;

    void streamerPrepared();
};

void ClientSession::Private::streamerPrepared()
{
    std::string sdp;
    gstClient.sdp(&sdp);
    if(sdp.empty()) {
        owner->disconnect();
        return;
    }

    owner->requestSetup(
        "http://example.com/",
        sdp,
        session);
}


void ClientSession::onConnected() noexcept
{
    requestOptions("*");
}

ClientSession::ClientSession(
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::ClientSession(sendRequest, sendResponse),
    _p(new Private { .owner = this })
{
}

ClientSession::~ClientSession()
{
}

bool ClientSession::onOptionsResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    requestDescribe("http://example.com/");

    return true;
}

bool ClientSession::onDescribeResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    _p->session = ResponseSession(response);
    if(_p->session.empty())
        return false;

    _p->gstClient.prepare(
        std::bind(
            &ClientSession::Private::streamerPrepared,
            _p.get()));

    _p->remoteSdp = response.body;
    if(_p->remoteSdp.empty())
        return false;

    _p->gstClient.setRemoteSdp(_p->remoteSdp);

    return true;
}

bool ClientSession::onSetupResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    if(ResponseSession(response) != _p->session)
        return false;

    requestPlay(request.uri, _p->session);

    return true;
}

bool ClientSession::onPlayResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    if(ResponseSession(response) != _p->session)
        return false;

    _p->gstClient.play();

    return true;
}

bool ClientSession::onTeardownResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(ResponseSession(response) != _p->session)
        return false;

    return false;
}
