#include "Forwarder.h"

#include <cassert>
#include <map>

#include "RtspParser/Common.h"

#include "FrontSession.h"
#include "BackSession.h"


// FIXME! need some protection from different session on the same address
struct Forwarder::Private
{
    Private(const AuthTokens& backAuthTokens);

    const AuthTokens backAuthTokens;
    std::map<std::string, BackSession*> backSessions;
};

Forwarder::Private::Private(const AuthTokens& backAuthTokens) :
    backAuthTokens(backAuthTokens)
{
}


Forwarder::Forwarder(const AuthTokens& backAuthTokens) :
    _p(std::make_unique<Private>(backAuthTokens))
{
}

Forwarder::~Forwarder()
{
}

std::unique_ptr<FrontSession>
Forwarder::createFrontSession(
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return
        std::make_unique<FrontSession>(
            this, sendRequest, sendResponse);
}

std::unique_ptr<BackSession>
Forwarder::createBackSession(
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return
        std::make_unique<BackSession>(
            this, sendRequest, sendResponse);
}

bool Forwarder::registerBackSession(
    const std::string& name,
    const std::string& token,
    BackSession* session)
{
    const auto it = _p->backAuthTokens.find(name);
    if(_p->backAuthTokens.end() == it || it->second != token)
        return false;

    const auto pair = _p->backSessions.emplace(name, session);
    if(!pair.second)
        return false;

    return true;
}

void Forwarder::removeBackSession(
    const std::string& name,
    BackSession* session)
{
    _p->backSessions.erase(name);
}

void Forwarder::registerMediaSession(
    FrontSession* frontSession,
    const rtsp::SessionId& frontMediaSession,
    BackSession* backSession,
    const rtsp::SessionId& backMediaSession)
{
    backSession->registerMediaSession(frontSession, frontMediaSession, backMediaSession);
}

void Forwarder::unregisterMediaSession(
    FrontSession* frontSession,
    const rtsp::SessionId& frontMediaSession,
    BackSession* backSession,
    const rtsp::SessionId& backMediaSession)
{
    backSession->unregisterMediaSession(frontSession, frontMediaSession, backMediaSession);
}

bool Forwarder::forwardToBackSession(
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

bool Forwarder::forwardToBackSession(
    BackSession* target,
    const rtsp::Response& response)
{
    return target->forward(response);
}

bool Forwarder::forwardToFrontSession(
    BackSession* source,
    FrontSession* target,
    std::unique_ptr<rtsp::Request>& requestPtr)
{
    return target->forward(source, requestPtr);
}

bool Forwarder::forwardToFrontSession(
    BackSession* source,
    FrontSession* target,
    const rtsp::Request& request,
    std::unique_ptr<rtsp::Response>& response)
{
    return target->forward(source, request, response);
}

void Forwarder::cancelRequest(
    BackSession* session,
    const rtsp::CSeq& cseq)
{
    session->cancelRequest(cseq);
}

void Forwarder::forceTeardown(
    BackSession* session,
    const rtsp::SessionId& mediaSession)
{
    session->forceTeardown(mediaSession);
}

void Forwarder::cancelRequest(
    FrontSession* session,
    const rtsp::CSeq& cseq)
{
    session->cancelRequest(cseq);
}

void Forwarder::dropSession(FrontSession* session)
{
    session->disconnect();
}
