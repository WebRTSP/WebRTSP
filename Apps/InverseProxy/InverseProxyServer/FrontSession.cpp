#include "FrontSession.h"

#include <cassert>
#include <map>

#include "RtspSession/StatusCode.h"

#include "GstStreaming/GstTestStreamer.h"

namespace {

struct RequestSource
{
    BackSession* source;
    rtsp::CSeq sourceCSeq;
    rtsp::SessionId session;
};

typedef std::map<rtsp::SessionId, rtsp::SessionId> BackSessionId2FrontSessionId;
typedef std::pair<BackSession*, rtsp::SessionId> BackSession2SessionId;

}

struct FrontSession::Private
{
    Private(FrontSession* owner, ForwardContext*);

    FrontSession *const owner;
    ForwardContext *const forwardContext;

    std::map<rtsp::CSeq, RequestSource> forwardRequests;

    std::map<BackSession*, BackSessionId2FrontSessionId> backSessionsIdx;
    std::map<rtsp::SessionId, BackSession2SessionId> sessionsIdx;

    std::string nextSessionId()
        { return std::to_string(_nextSessionId++); }

private:
    unsigned _nextSessionId = 1;
};

FrontSession::Private::Private(
    FrontSession* owner, ForwardContext* forwardContext) :
    owner(owner), forwardContext(forwardContext)
{
}


FrontSession::FrontSession(
    ForwardContext* forwardContext,
    const std::function<void (const rtsp::Request*)>& sendRequest,
    const std::function<void (const rtsp::Response*)>& sendResponse) noexcept :
    rtsp::Session(sendRequest, sendResponse),
    _p(new Private(this, forwardContext))
{
}

FrontSession::~FrontSession()
{
}

bool FrontSession::handleRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    const rtsp::SessionId sessionId = rtsp::RequestSession(*requestPtr);
    BackSession* targetSession = nullptr;
    if(!sessionId.empty()) {
        const auto sessionIt = _p->sessionsIdx.find(sessionId);
        if(_p->sessionsIdx.end() == sessionIt)
            return false;

        const BackSession2SessionId& back2backId = sessionIt->second;
        targetSession = back2backId.first;

        rtsp::SetRequestSession(requestPtr.get(), back2backId.second);
    }

    return _p->forwardContext->forwardToBackSession(this, targetSession, requestPtr);
}

bool FrontSession::forward(
    BackSession* source,
    std::unique_ptr<rtsp::Request>& sourceRequestPtr)
{
    rtsp::Request& sourceRequest = *sourceRequestPtr;
    const rtsp::CSeq sourceCSeq = sourceRequest.cseq;
    const rtsp::SessionId sourceSessionId = rtsp::RequestSession(sourceRequest);

    rtsp::Request* request =
        createRequest(sourceRequest.method, sourceRequest.uri);
    const rtsp::CSeq requestCSeq = request->cseq;

    const bool inserted =
        _p->forwardRequests.emplace(
            requestCSeq,
            RequestSource { source, sourceCSeq, sourceSessionId }).second;
    assert(inserted);

    request->headerFields.swap(sourceRequest.headerFields);
    request->body.swap(sourceRequest.body);
    sourceRequestPtr.reset();

    auto it = _p->backSessionsIdx.find(source);
    if(it != _p->backSessionsIdx.end() && !sourceSessionId.empty()) {
        BackSessionId2FrontSessionId& backId2FrontId = it->second;

        auto sessionIdIt = backId2FrontId.find(sourceSessionId);
        if(sessionIdIt != backId2FrontId.end()) {
            rtsp::SessionId localSessionId = sessionIdIt->second;
            rtsp::SetRequestSession(request, localSessionId);
        }
    }

    sendRequest(*request);

    return true;
}

bool FrontSession::forward(
    BackSession* source,
    const rtsp::Request& request,
    std::unique_ptr<rtsp::Response>& responsePtr)
{
    const rtsp::SessionId sourceSessionId = rtsp::ResponseSession(*responsePtr);

    if(rtsp::Method::DESCRIBE == request.method &&
       rtsp::StatusCode::OK == responsePtr->statusCode)
    {
        BackSessionId2FrontSessionId& back2front = _p->backSessionsIdx[source];

        const rtsp::SessionId localSessionId = _p->nextSessionId();
        back2front.emplace(sourceSessionId, localSessionId);

        BackSession2SessionId& back2backId = _p->sessionsIdx[localSessionId];
        back2backId.first = source;
        back2backId.second = sourceSessionId;
    }

    rtsp::SessionId localSessionId;

    auto it = _p->backSessionsIdx.find(source);
    if(it != _p->backSessionsIdx.end() && !sourceSessionId.empty()) {
        BackSessionId2FrontSessionId& back2front = it->second;

        auto sessionIdIt = back2front.find(sourceSessionId);
        if(sessionIdIt != back2front.end()) {
            localSessionId = sessionIdIt->second;
            rtsp::SetResponseSession(responsePtr.get(), localSessionId);
        }
    }

    sendResponse(*responsePtr);
    responsePtr.reset();

    if(rtsp::Method::TEARDOWN == request.method) {
        if(it != _p->backSessionsIdx.end()) {
            BackSessionId2FrontSessionId& back2front = it->second;

            back2front.erase(sourceSessionId);

            if(back2front.empty())
                _p->backSessionsIdx.erase(it);
        }

        _p->sessionsIdx.erase(localSessionId);
    }

    return true;
}

bool FrontSession::handleResponse(
    const rtsp::Request& request,
    std::unique_ptr<rtsp::Response>& responsePtr) noexcept
{
    const rtsp::SessionId requestSessionId = rtsp::RequestSession(request);
    const rtsp::SessionId responseSessionId = rtsp::ResponseSession(*responsePtr);
    if(requestSessionId != responseSessionId)
        return false;

    BackSession* targetSession = nullptr;
    rtsp::SessionId targetSessionId;
    if(!responseSessionId.empty()) {
        const auto sessionIt = _p->sessionsIdx.find(responseSessionId);
        if(_p->sessionsIdx.end() == sessionIt)
            return false;

        const BackSession2SessionId& back2backId = sessionIt->second;
        targetSession = back2backId.first;
        targetSessionId = back2backId.second;

        rtsp::SetResponseSession(responsePtr.get(), targetSessionId);
    }

    const auto requestIt = _p->forwardRequests.find(responsePtr->cseq);
    if(requestIt == _p->forwardRequests.end())
        return false;

    const RequestSource& requestSource = requestIt->second;

    if(requestSource.session != targetSessionId)
        return false;

    responsePtr->cseq = requestSource.sourceCSeq;

    BackSession* targetSession2 = requestSource.source;
    if(targetSession && targetSession != targetSession2)
        return false;

    return _p->forwardContext->forwardToBackSession(targetSession2, *responsePtr);
}
