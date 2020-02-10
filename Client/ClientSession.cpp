#include "ClientSession.h"

#include "RtspSession/StatusCode.h"

#include "Log.h"


static const auto Log = ClientSessionLog;


struct ClientSession::Private
{
    Private(
        ClientSession* owner,
        const std::string& uri,
        std::function<std::unique_ptr<WebRTCPeer> ()> createPeer);

    ClientSession* owner;

    const std::string uri;

    std::unique_ptr<WebRTCPeer> client;
    std::string remoteSdp;
    rtsp::SessionId session;

    void streamerPrepared();
    void iceCandidate(unsigned, const std::string&);
    void eos();
};

ClientSession::Private::Private(
    ClientSession* owner,
    const std::string& uri,
    std::function<std::unique_ptr<WebRTCPeer> ()> createPeer) :
    owner(owner), uri(uri), client(createPeer())
{
}

void ClientSession::Private::streamerPrepared()
{
    std::string sdp;
    client->sdp(&sdp);
    if(sdp.empty()) {
        owner->disconnect();
        return;
    }

    owner->requestSetup(
        uri,
        "application/sdp",
        session,
        sdp);
}

void ClientSession::Private::iceCandidate(
    unsigned mlineIndex, const std::string& candidate)
{
    owner->requestSetup(
        uri,
        "application/x-ice-candidate",
        session,
        std::to_string(mlineIndex) + "/" + candidate + "\r\n");
}

void ClientSession::Private::eos()
{
    Log()->trace("Eos");
}


bool ClientSession::onConnected() noexcept
{
    requestOptions(_p->uri);

    return true;
}

ClientSession::ClientSession(
    const std::string& uri,
    const std::function<std::unique_ptr<WebRTCPeer> ()>& createPeer,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::ClientSession(sendRequest, sendResponse),
    _p(new Private(this, uri, createPeer))
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

    requestDescribe(_p->uri);

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

    _p->client->prepare(
        WebRTCPeer::IceServers(),
        std::bind(
            &ClientSession::Private::streamerPrepared,
            _p.get()),
        std::bind(
            &ClientSession::Private::iceCandidate,
            _p.get(),
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(
            &ClientSession::Private::eos,
            _p.get()));

    _p->remoteSdp = response.body;
    if(_p->remoteSdp.empty())
        return false;

    _p->client->setRemoteSdp(_p->remoteSdp);

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

    if(RequestContentType(request) == "application/sdp")
        requestPlay(_p->uri, _p->session);

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

    _p->client->play();

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

bool ClientSession::onSetupRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    if(RequestSession(*requestPtr) != _p->session)
        return false;

    if(RequestContentType(*requestPtr) != "application/x-ice-candidate")
        return false;

    const std::string& ice = requestPtr->body;

    const std::string::size_type delimiterPos = ice.find("/");
    if(delimiterPos == std::string::npos || 0 == delimiterPos)
        return false;

    const std::string::size_type lineEndPos = ice.find("\r\n", delimiterPos + 1);
    if(lineEndPos == std::string::npos)
        return false;

    try{
        int idx = std::stoi(ice.substr(0, delimiterPos - 0));
        if(idx < 0)
            return false;

        const std::string candidate =
            ice.substr(delimiterPos + 1, lineEndPos - (delimiterPos + 1));

        if(candidate.empty())
            return false;

        if(candidate == "a=end-of-candidates")
            ;
        else
            _p->client->addIceCandidate(idx, candidate);

        sendOkResponse(requestPtr->cseq, rtsp::RequestSession(*requestPtr));

        return true;
    } catch(...) {
        return false;
    }
}

