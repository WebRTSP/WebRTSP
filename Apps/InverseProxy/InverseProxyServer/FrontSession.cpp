#include "FrontSession.h"

#include <cassert>
#include <map>

#include "RtspSession/StatusCode.h"

#include "GstStreaming/GstTestStreamer.h"

#include "Log.h"


namespace {

struct RequestSource
{
    BackSession* source;
    rtsp::CSeq sourceCSeq;
};
typedef std::map<rtsp::CSeq, RequestSource> ForwardRequests;

typedef std::pair<BackSession*, rtsp::SessionId> BackMediaSession;

const auto Log = InverseProxyServerLog;

}

struct FrontSession::Private
{
    Private(FrontSession* owner, Forwarder*);

    FrontSession *const owner;
    Forwarder *const forwarder;

    ForwardRequests forwardRequests;
    struct AutoEraseForwardRequest;

    std::map<rtsp::SessionId, BackMediaSession> mediaSessions;

    std::string nextMediaSession()
        { return std::to_string(_nextMediaSession++); }

private:
    unsigned _nextMediaSession = 1;
};

FrontSession::Private::Private(
    FrontSession* owner, Forwarder* forwarder) :
    owner(owner), forwarder(forwarder)
{
}


struct FrontSession::Private::AutoEraseForwardRequest
{
    AutoEraseForwardRequest(
        Private* owner,
        ForwardRequests::const_iterator it) :
        _owner(owner), _it(it) {}
    ~AutoEraseForwardRequest()
        { if(_owner) _owner->forwardRequests.erase(_it); }
    void discard()
        { _owner = nullptr; }

private:
    Private* _owner;
    ForwardRequests::const_iterator _it;
};


FrontSession::FrontSession(
    Forwarder* forwarder,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::Session(sendRequest, sendResponse),
    _p(new Private(this, forwarder))
{
}

FrontSession::~FrontSession()
{
    for(const auto pair: _p->forwardRequests) {
        const RequestSource& source = pair.second;
        _p->forwarder->cancelRequest(
            source.source, source.sourceCSeq);
    }

    for(const auto pair: _p->mediaSessions) {
        const rtsp::SessionId mediaSession = pair.first;
        const BackMediaSession& backMediaSession = pair.second;
        _p->forwarder->forceTeardown(
            backMediaSession.first, backMediaSession.second);
    }
}

bool FrontSession::handleRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId sessionId = rtsp::RequestSession(*requestPtr);
    BackSession* target = nullptr;
    if(!sessionId.empty()) {
        const auto sessionIt = _p->mediaSessions.find(sessionId);
        if(_p->mediaSessions.end() == sessionIt)
            return false;

        const BackMediaSession& targetSession = sessionIt->second;
        target = targetSession.first;

        rtsp::SetRequestSession(requestPtr.get(), targetSession.second);
    }

    return _p->forwarder->forwardToBackSession(this, target, requestPtr);
}

bool FrontSession::handleResponse(
    const rtsp::Request& request,
    std::unique_ptr<rtsp::Response>& responsePtr) noexcept
{
    const rtsp::SessionId requestMediaSession = rtsp::RequestSession(request);
    const rtsp::SessionId responseMediaSession = rtsp::ResponseSession(*responsePtr);
    if(requestMediaSession != responseMediaSession) {
        Log()->error(
            "FrontSession: request and response have different media sessions.");
        return false;
    }

    BackSession* mediaSessionTarget = nullptr;
    if(!responseMediaSession.empty()) {
        const auto mediaSessionIt = _p->mediaSessions.find(responseMediaSession);
        if(_p->mediaSessions.end() == mediaSessionIt) {
            Log()->error(
                "FrontSession: unknown media session({}).",
                responseMediaSession);
            return false;
        }

        const BackMediaSession& targetMediaSession = mediaSessionIt->second;
        mediaSessionTarget = targetMediaSession.first;

        rtsp::SetResponseSession(responsePtr.get(), targetMediaSession.second);
    }

    const auto requestIt = _p->forwardRequests.find(responsePtr->cseq);
    if(requestIt == _p->forwardRequests.end()) {
        Log()->error(
            "FrontSession: Fail find forwarded request by CSeq({}).",
            responsePtr->cseq);
        return false;
    }

    Private::AutoEraseForwardRequest autoEraseRequest(_p.get(), requestIt);

    const RequestSource& requestSource = requestIt->second;

    responsePtr->cseq = requestSource.sourceCSeq;

    BackSession* targetSession = requestSource.source;
    if(mediaSessionTarget && mediaSessionTarget != targetSession) {
        Log()->error(
            "FrontSession: media session target differs from target.",
            responsePtr->cseq);
        return false;
    }

    return _p->forwarder->forwardToBackSession(targetSession, *responsePtr);
}

bool FrontSession::forward(
    BackSession* source,
    std::unique_ptr<rtsp::Request>& sourceRequestPtr)
{
    rtsp::Request& sourceRequest = *sourceRequestPtr;
    const rtsp::CSeq sourceCSeq = sourceRequest.cseq;

    rtsp::Request* request =
        createRequest(sourceRequest.method, sourceRequest.uri);
    const rtsp::CSeq requestCSeq = request->cseq;

    const bool inserted =
        _p->forwardRequests.emplace(
            requestCSeq,
            RequestSource { source, sourceCSeq }).second;
    assert(inserted);

    request->headerFields.swap(sourceRequest.headerFields);
    request->body.swap(sourceRequest.body);
    sourceRequestPtr.reset();

    sendRequest(*request);

    return true;
}

bool FrontSession::forward(
    BackSession* source,
    const rtsp::Request& request,
    std::unique_ptr<rtsp::Response>& responsePtr)
{
    const rtsp::SessionId responseMediaSession = rtsp::ResponseSession(*responsePtr);

    if(rtsp::Method::DESCRIBE == request.method) {
        if(rtsp::StatusCode::OK == responsePtr->statusCode) {
            const rtsp::SessionId frontMediaSession = _p->nextMediaSession();

            _p->forwarder->registerMediaSession(
                this, frontMediaSession,
                source, responseMediaSession);

            _p->mediaSessions.emplace(
                frontMediaSession,
                BackMediaSession { source, responseMediaSession });

            rtsp::SetResponseSession(responsePtr.get(), frontMediaSession);
        }
    }

    sendResponse(*responsePtr);
    responsePtr.reset();

    if(rtsp::Method::TEARDOWN == request.method) {
        const auto it = _p->mediaSessions.find(responseMediaSession);
        if(it != _p->mediaSessions.end()) {
            const BackMediaSession& mediaSession = it->second;

            assert(source == mediaSession.first);

            _p->forwarder->unregisterMediaSession(
                this, responseMediaSession,
                source, mediaSession.second);

            _p->mediaSessions.erase(it);
        }
    }

    return true;
}

void FrontSession::cancelRequest(const rtsp::CSeq& cseq)
{
    rtsp::Response response;
    prepareResponse(
        rtsp::StatusCode::BAD_GATEWAY, "Bad gateway",
        cseq, rtsp::SessionId(), &response);
    sendResponse(response);
}
