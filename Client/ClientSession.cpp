#include "ClientSession.h"

#include "RtspSession/StatusCode.h"


struct ClientSession::Private
{
    ClientSession* owner;

    GstClient gstClient;
    std::string remoteSdp;

    void streamerPrepared();
};

void ClientSession::Private::streamerPrepared()
{
    std::string sdp;
    gstClient.sdp(&sdp);

    owner->requestSetup("http://example.com/", sdp);
}

void ClientSession::onConnected() noexcept
{
    requestOptions("*");
}

ClientSession::ClientSession(
    const std::function<void (const rtsp::Request*)>& cb) :
    rtsp::ClientSession(cb), _p(new Private { .owner = this })
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

    _p->gstClient.prepare(
        std::bind(&ClientSession::Private::streamerPrepared, _p.get()));

    return true;
}

bool ClientSession::onDescribeResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

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

    requestPlay(request.uri, "fake_session_id");

    return true;
}

bool ClientSession::onPlayResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    _p->gstClient.play();

    return true;
}

bool ClientSession::onTeardownResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    return false;
}
