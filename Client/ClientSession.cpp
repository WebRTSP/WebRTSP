#include "ClientSession.h"

#include "RtspSession/StatusCode.h"

#include "Log.h"


static const auto Log = ClientSessionLog;


struct ClientSession::Private
{
    Private(
        ClientSession* owner,
        const std::string& uri,
        const CreatePeer& createPeer);

    ClientSession* owner;

    std::string uri;

    std::unique_ptr<WebRTCPeer> receiver;
    rtsp::MediaSessionId session;

    void receiverPrepared();
    void iceCandidate(unsigned, const std::string&);
    void eos();
};

ClientSession::Private::Private(
    ClientSession* owner,
    const std::string& uri,
    const CreatePeer& createPeer) :
    owner(owner), uri(uri), receiver(createPeer())
{
}

void ClientSession::Private::receiverPrepared()
{
    if(receiver->sdp().empty()) {
        owner->disconnect();
        return;
    }

    owner->requestPlay(
        uri,
        session,
        receiver->sdp());
}

void ClientSession::Private::iceCandidate(
    unsigned mlineIndex, const std::string& candidate)
{
    owner->requestSetup(
        uri,
        rtsp::IceCandidateContentType,
        session,
        std::to_string(mlineIndex) + "/" + candidate + "\r\n");
}

void ClientSession::Private::eos()
{
    Log()->trace("[{}] Eos", owner->sessionLogId);

    owner->onEos(); // FIXME! send TEARDOWN and remove Media Session instead
}


ClientSession::ClientSession(
    const std::string& uri,
    const WebRTCConfigPtr& webRTCConfig,
    const CreatePeer& createPeer,
    const SendRequest& sendRequest,
    const SendResponse& sendResponse) noexcept :
    rtsp::ClientSession(webRTCConfig, sendRequest, sendResponse),
    _p(new Private(this, uri, createPeer))
{
}

ClientSession::ClientSession(
    const WebRTCConfigPtr& webRTCConfig,
    const CreatePeer& createPeer,
    const SendRequest& sendRequest,
    const SendResponse& sendResponse) noexcept :
    ClientSession(std::string(), webRTCConfig, createPeer, sendRequest, sendResponse)
{
}

ClientSession::~ClientSession()
{
}

void ClientSession::setUri(const std::string& uri)
{
    _p->uri = uri;
}

bool ClientSession::onConnected() noexcept
{
    requestOptions(!_p->uri.empty() ? _p->uri : "*");

    return true;
}

rtsp::CSeq ClientSession::requestDescribe() noexcept
{
    assert(!_p->uri.empty());
    return rtsp::ClientSession::requestDescribe(_p->uri);
}

bool ClientSession::onOptionsResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(!rtsp::ClientSession::onOptionsResponse(request, response))
        return false;

    if(!_p->uri.empty())
        requestDescribe();

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

    const std::string& sdp = response.body;
    if(sdp.empty())
        return false;

    _p->receiver->prepare(
        webRTCConfig(),
        std::bind(
            &ClientSession::Private::receiverPrepared,
            _p.get()),
        std::bind(
            &ClientSession::Private::iceCandidate,
            _p.get(),
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(
            &ClientSession::Private::eos,
            _p.get()));


    _p->receiver->setRemoteSdp(sdp);

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

    _p->receiver->play();

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

    if(RequestContentType(*requestPtr) != rtsp::IceCandidateContentType)
        return false;

    const std::string& ice = requestPtr->body;

    std::string::size_type pos = 0;
    while(pos < ice.size()) {
        const std::string::size_type lineEndPos = ice.find("\r\n", pos);
        if(lineEndPos == std::string::npos)
            return false;

        const std::string line = ice.substr(pos, lineEndPos - pos);

        const std::string::size_type delimiterPos = line.find("/");
        if(delimiterPos == std::string::npos || 0 == delimiterPos)
            return false;

        try{
            const int idx = std::stoi(line.substr(0, delimiterPos));
            if(idx < 0)
                return false;

            const std::string candidate =
                line.substr(delimiterPos + 1);

            if(candidate.empty())
                return false;

            Log()->trace("[{}] Adding ice candidate \"{}\"", sessionLogId, candidate);

            _p->receiver->addIceCandidate(idx, candidate);
        } catch(...) {
            return false;
        }
        pos = lineEndPos + 2;
    }

    sendOkResponse(requestPtr->cseq, rtsp::RequestSession(*requestPtr));

    return true;
}
