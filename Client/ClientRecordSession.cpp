#include "ClientRecordSession.h"

#include "RtspSession/StatusCode.h"

#include "Log.h"


static const auto Log = ClientSessionLog;

namespace {

struct IceCandidate {
    unsigned mlineIndex;
    std::string candidate;
};

}

struct ClientRecordSession::Private
{
    Private(
        ClientRecordSession* owner,
        const std::string& uri,
        std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> createPeer);

    ClientRecordSession* owner;

    std::string uri;
    std::string recordToken;

    std::unique_ptr<WebRTCPeer> streamer;
    std::deque<IceCandidate> iceCandidates;
    rtsp::SessionId session;

    void streamerPrepared();
    void iceCandidate(unsigned, const std::string&);
    void eos();
};

ClientRecordSession::Private::Private(
    ClientRecordSession* owner,
    const std::string& uri,
    std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> createPeer) :
    owner(owner), uri(uri), streamer(createPeer(uri))
{
}

void ClientRecordSession::Private::streamerPrepared()
{
    if(streamer->sdp().empty()) {
        owner->disconnect();
        return;
    }

    owner->requestRecord(uri, streamer->sdp(), recordToken);
}

void ClientRecordSession::Private::iceCandidate(
    unsigned mlineIndex, const std::string& candidate)
{
    if(session.empty()) {
        iceCandidates.emplace_back(IceCandidate { mlineIndex, candidate });
    } else {
        owner->requestSetup(
            uri,
            "application/x-ice-candidate",
            session,
            std::to_string(mlineIndex) + "/" + candidate + "\r\n");
    }
}

void ClientRecordSession::Private::eos()
{
    Log()->trace("Eos");

    owner->onEos();
}


ClientRecordSession::ClientRecordSession(
    const std::string& uri,
    const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::ClientSession(sendRequest, sendResponse),
    _p(new Private(this, uri, createPeer))
{
}

ClientRecordSession::~ClientRecordSession()
{
}

void ClientRecordSession::setUri(const std::string& uri)
{
    _p->uri = uri;
}

bool ClientRecordSession::onConnected() noexcept
{
    requestOptions(!_p->uri.empty() ? _p->uri : "*");

    return true;
}

bool ClientRecordSession::onRecordResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(response.statusCode != rtsp::StatusCode::OK)
        return false;

    if(ResponseContentType(response) != "application/sdp")
        return false;

    rtsp::SessionId session = ResponseSession(response);
    if(session.empty())
        return false;

    _p->streamer->setRemoteSdp(response.body);

    _p->session = session;

    if(!_p->iceCandidates.empty()) {
        std::string iceCandidates;
        for(const IceCandidate& c : _p->iceCandidates) {
            iceCandidates +=
                std::to_string(c.mlineIndex) + "/" + c.candidate + "\r\n";
        }

        if(!iceCandidates.empty()) {
            requestSetup(
                _p->uri,
                "application/x-ice-candidate",
                _p->session,
                iceCandidates);
        }

        _p->iceCandidates.clear();
    }

    _p->streamer->play();

    return true;
}

bool ClientRecordSession::onSetupResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    if(ResponseSession(response) != _p->session)
        return false;

    const std::string contentType = RequestContentType(request);
     if(contentType == "application/x-ice-candidate")
        ;
     else
         return false;

    return true;
}

bool ClientRecordSession::onTeardownResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(ResponseSession(response) != _p->session)
        return false;

    return false;
}

bool ClientRecordSession::onSetupRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
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

        _p->streamer->addIceCandidate(idx, candidate);

        sendOkResponse(requestPtr->cseq, rtsp::RequestSession(*requestPtr));

        return true;
    } catch(...) {
        return false;
    }
}

void ClientRecordSession::startRecord(const std::string& recordToken)
{
    _p->recordToken = recordToken;
    _p->streamer->prepare(
        WebRTCPeer::IceServers(),
        std::bind(
            &ClientRecordSession::Private::streamerPrepared,
            _p.get()),
        std::bind(
            &ClientRecordSession::Private::iceCandidate,
            _p.get(),
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(
            &ClientRecordSession::Private::eos,
            _p.get()));
}
