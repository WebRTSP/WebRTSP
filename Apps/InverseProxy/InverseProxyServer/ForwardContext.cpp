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

bool ForwardContext::forwardToBackSession(
    FrontSession* source,
    std::unique_ptr<rtsp::Request>& requestPtr)
{
    const auto backSessionIt = _p->backSessions.find(requestPtr->uri);
    if(backSessionIt == _p->backSessions.end())
        return false;

    BackSession* backSession = backSessionIt->second;
    return backSession->forward(source, requestPtr);
}

bool ForwardContext::forwardToBackSession(
    BackSession* targetSession,
    const rtsp::Response& response)
{
    return targetSession->forward(response);
}

bool ForwardContext::forwardToFrontSession(
    BackSession* requestSource,
    FrontSession* requestTarget,
    std::unique_ptr<rtsp::Request>& requestPtr)
{
    return requestTarget->forward(requestSource, requestPtr);
}

bool ForwardContext::forwardToFrontSession(
    FrontSession* targetSession,
    const rtsp::Response& response)
{
    return targetSession->forward(response);
}
