#include "ClientRecordSession.h"

#include "RtspSession/StatusCode.h"
#include "RtspSession/IceCandidate.h"

#include "Log.h"


static const auto Log = ClientSessionLog;

struct ClientRecordSession::Private
{
    Private(
        ClientRecordSession* owner,
        const std::string& targetUri,
        const std::string& recordToken,
        std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> createPeer);

    ClientRecordSession* owner;

    std::string targetUri;
    std::string sourceUri;
    std::string recordToken;

    const CreatePeer createPeer;

    std::unique_ptr<WebRTCPeer> streamer;
    std::deque<rtsp::IceCandidate> iceCandidates;
    rtsp::SessionId session;

    void streamerPrepared();
    void iceCandidate(unsigned, const std::string&);
    void eos();
};

ClientRecordSession::Private::Private(
    ClientRecordSession* owner,
    const std::string& targetUri,
    const std::string& recordToken,
    std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)> createPeer) :
    owner(owner), targetUri(targetUri), recordToken(recordToken), createPeer(createPeer)
{
}

void ClientRecordSession::Private::streamerPrepared()
{
    if(streamer->sdp().empty()) {
        owner->disconnect();
        return;
    }

    owner->requestRecord(targetUri, streamer->sdp(), recordToken);
}

void ClientRecordSession::Private::iceCandidate(
    unsigned mlineIndex, const std::string& candidate)
{
    if(session.empty()) {
        iceCandidates.emplace_back(rtsp::IceCandidate { mlineIndex, candidate });
    } else {
        owner->requestSetup(
            targetUri,
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
    const std::string& targetUri,
    const std::string& recordToken,
    const IceServers& iceServers,
    const std::function<std::unique_ptr<WebRTCPeer> (const std::string& uri)>& createPeer,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::ClientSession(iceServers, sendRequest, sendResponse),
    _p(new Private(this, targetUri, recordToken, createPeer))
{
}

ClientRecordSession::~ClientRecordSession()
{
}

bool ClientRecordSession::onConnected() noexcept
{
    requestOptions(!_p->targetUri.empty() ? _p->targetUri : "*");

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
        for(const rtsp::IceCandidate& c : _p->iceCandidates) {
            iceCandidates +=
                std::to_string(c.mlineIndex) + "/" + c.candidate + "\r\n";
        }

        if(!iceCandidates.empty()) {
            requestSetup(
                _p->targetUri,
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

    return true;
}

bool ClientRecordSession::onSetupRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    if(RequestSession(*requestPtr) != _p->session)
        return false;

    if(RequestContentType(*requestPtr) != "application/x-ice-candidate")
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

            Log()->trace("Adding ice candidate \"{}\"", candidate);

            _p->streamer->addIceCandidate(idx, candidate);
        } catch(...) {
            return false;
        }
        pos = lineEndPos + 2;
    }

    sendOkResponse(requestPtr->cseq, rtsp::RequestSession(*requestPtr));

    return true;
}

void ClientRecordSession::startRecord(const std::string& sourceUri)
{
    assert(!_p->streamer);
    if(_p->streamer) {
        return;
    }

    _p->streamer = _p->createPeer(sourceUri);

    assert(_p->streamer);
    if(!_p->streamer) {
        return;
    }

    _p->streamer->prepare(
        iceServers(),
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
