#include "ClientRecordSession.h"

#include "RtspParser/RtspParser.h"

#include "RtspSession/StatusCode.h"
#include "RtspSession/IceCandidate.h"

#include "Log.h"


#define SESSION "[{}]" " "
#define TAG "[ClientRecordSession]" " "


namespace rtsp {

namespace {

inline bool IsMethodSupported(
    const std::set<Method>& supportedMethods,
    Method method) noexcept
{
    return supportedMethods.find(method) != supportedMethods.end();
}

bool IsRecordSupported(const std::set<Method>& supportedMethods) noexcept
{
    return
        IsMethodSupported(supportedMethods, Method::RECORD) &&
        IsMethodSupported(supportedMethods, Method::SETUP) &&
        IsMethodSupported(supportedMethods, Method::TEARDOWN);
}

}


struct ClientRecordSession::Private
{
    Private(
        ClientRecordSession* owner,
        const std::string& targetUri,
        const std::string& recordToken,
        const CreatePeer& createPeer);

    ClientRecordSession* owner;

    const std::string targetUri;
    const std::string sourceUri;
    const std::string recordToken;

    const CreatePeer createPeer;

    std::unique_ptr<WebRTCPeer> streamer;
    std::deque<IceCandidate> iceCandidates;

    CSeq recordRequested = false;
    MediaSessionId session;

    void streamerPrepared();
    void iceCandidate(unsigned, const std::string&);
    void eos();
};

ClientRecordSession::Private::Private(
    ClientRecordSession* owner,
    const std::string& targetUri,
    const std::string& recordToken,
    const CreatePeer& createPeer) :
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
    recordRequested = true;
}

void ClientRecordSession::Private::iceCandidate(
    unsigned mlineIndex, const std::string& candidate)
{
    if(session.empty()) {
        iceCandidates.emplace_back(IceCandidate { mlineIndex, candidate });
    } else {
        owner->requestSetup(
            targetUri,
            IceCandidateContentType,
            session,
            std::to_string(mlineIndex) + "/" + candidate + "\r\n");
    }
}

void ClientRecordSession::Private::eos()
{
    owner->log()->trace(SESSION TAG "Eos", owner->sessionLogId);

    owner->onEos(); // FIXME! send TEARDOWN and remove Media Session instead
}


ClientRecordSession::ClientRecordSession(
    const std::string& targetUri,
    const std::string& recordToken,
    const WebRTCConfigPtr& webRTCConfig,
    const CreatePeer& createPeer,
    const SendRequest& sendRequest,
    const SendResponse& sendResponse) noexcept :
    Session(sendRequest, sendResponse),
    WebRTCSessionMixin(webRTCConfig),
    _p(new Private(this, targetUri, recordToken, createPeer))
{
}

ClientRecordSession::~ClientRecordSession()
{
}

bool ClientRecordSession::onConnected() noexcept
{
    requestOptions(!_p->targetUri.empty() ? _p->targetUri : WildcardUri);

    return true;
}

bool ClientRecordSession::onOptionsResponse(
    const Request& request,
    const Response& response) noexcept
{
    if(StatusCode::OK != response.statusCode)
        return false;

    const std::set<Method> supportedMethods = ParseOptions(response);

    return IsRecordSupported(supportedMethods);
}

bool ClientRecordSession::onRecordResponse(
    const Request& request,
    const Response& response) noexcept
{
    if(response.statusCode != StatusCode::OK)
        return false;

    if(response.contentType != SdpContentType)
        return false;

    MediaSessionId session = response.session;
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
                _p->targetUri,
                IceCandidateContentType,
                _p->session,
                std::move(iceCandidates));
        }

        _p->iceCandidates.clear();
    }

    _p->streamer->play();

    return true;
}

bool ClientRecordSession::onSetupResponse(
    const Request& request,
    const Response& response) noexcept
{
    if(StatusCode::OK != response.statusCode)
        return false;

    if(response.session != _p->session)
        return false;

    const std::string contentType = request.contentType;
     if(contentType == IceCandidateContentType)
        ;
     else
         return false;

    return true;
}

bool ClientRecordSession::onTeardownResponse(
    const Request& request,
    const Response& response) noexcept
{
    return true;
}

bool ClientRecordSession::onSetupRequest(std::unique_ptr<Request>&& requestPtr) noexcept
{
    if(requestPtr->session != _p->session)
        return false;

    if(requestPtr->contentType != IceCandidateContentType)
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

            log()->trace(
                SESSION TAG "Adding ice candidate \"{}\"",
                sessionLogId,
                candidate);

            _p->streamer->addIceCandidate(idx, candidate);
        } catch(...) {
            return false;
        }
        pos = lineEndPos + 2;
    }

    sendOkResponse(requestPtr->cseq, requestPtr->session);

    return true;
}

bool ClientRecordSession::isStreaming() const noexcept
{
    return _p->streamer != nullptr;
}

void ClientRecordSession::startRecord(const std::string& sourceUri) noexcept
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

    assert(!_p->recordRequested);

    _p->streamer->prepare(
        webRTCConfig(),
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
            _p.get()),
        sessionLogId);
}

void ClientRecordSession::stopRecord() noexcept
{
    if(!_p->streamer) {
        assert(!_p->recordRequested);
        return;
    }

    _p->streamer->stop();
    _p->streamer.reset();

    if(_p->recordRequested) {
        assert(!_p->session.empty());
        if(!_p->session.empty()) {
            requestTeardown(_p->targetUri, _p->session);
            _p->session.clear();
            _p->recordRequested = false;
        } else {
            // FIXME? didn't get response to RECORD request yet
            disconnect();
        }
    }
}

}
