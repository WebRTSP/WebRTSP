#include "ForwardContext.h"

#include <cassert>
#include <map>

#include "RtspParser/Common.h"

#include "FrontSession.h"
#include "BackSession.h"


// FIXME! need some protection from different session on the same address
struct ForwardContext::Private
{
    std::map<std::string, BackSession*> backSessions;
};


ForwardContext::ForwardContext() :
    _p(std::make_unique<Private>())
{
}

ForwardContext::~ForwardContext()
{
}

std::unique_ptr<FrontSession>
ForwardContext::createFrontSession(
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return
        std::make_unique<FrontSession>(
            this, sendRequest, sendResponse);
}

std::unique_ptr<BackSession>
ForwardContext::createBackSession(
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return
        std::make_unique<BackSession>(
            this, sendRequest, sendResponse);
}

bool ForwardContext::registerBackSession(
    const std::string& name,
    BackSession* session)
{
    const auto pair = _p->backSessions.emplace(name, session);
    if(!pair.second)
        return false;

    return true;
}

void ForwardContext::removeBackSession(
    const std::string& name,
    BackSession* session)
{
    _p->backSessions.erase(name);
}

void ForwardContext::registerMediaSession(
    FrontSession* frontSession,
    const rtsp::SessionId& frontMediaSession,
    BackSession* backSession,
    const rtsp::SessionId& backMediaSession)
{
    backSession->registerMediaSession(frontSession, frontMediaSession, backMediaSession);
}

void ForwardContext::unregisterMediaSession(
    FrontSession* frontSession,
    const rtsp::SessionId& frontMediaSession,
    BackSession* backSession,
    const rtsp::SessionId& backMediaSession)
{
    backSession->unregisterMediaSession(frontSession, frontMediaSession, backMediaSession);
}

bool ForwardContext::forwardToBackSession(
    FrontSession* source,
    BackSession* target,
    std::unique_ptr<rtsp::Request>& requestPtr)
{
    const auto backSessionIt = _p->backSessions.find(requestPtr->uri);
    if(backSessionIt == _p->backSessions.end())
        return false;

    BackSession* target2 = backSessionIt->second;

    if(target && target != target2)
        return false;

    return target2->forward(source, requestPtr);
}

bool ForwardContext::forwardToBackSession(
    BackSession* target,
    const rtsp::Response& response)
{
    return target->forward(response);
}

bool ForwardContext::forwardToFrontSession(
    BackSession* source,
    FrontSession* target,
    std::unique_ptr<rtsp::Request>& requestPtr)
{
    return target->forward(source, requestPtr);
}

bool ForwardContext::forwardToFrontSession(
    BackSession* source,
    FrontSession* target,
    const rtsp::Request& request,
    std::unique_ptr<rtsp::Response>& response)
{
    return target->forward(source, request, response);
}

void ForwardContext::cancelRequest(
    BackSession* session,
    const rtsp::CSeq& cseq)
{
    session->cancelRequest(cseq);
}

void ForwardContext::forceTeardown(
    BackSession* session,
    const rtsp::SessionId& mediaSession)
{
    session->forceTeardown(mediaSession);
}

void ForwardContext::cancelRequest(
    FrontSession* session,
    const rtsp::CSeq& cseq)
{
    session->cancelRequest(cseq);
}

void ForwardContext::dropSession(FrontSession* session)
{
    session->disconnect();
}
