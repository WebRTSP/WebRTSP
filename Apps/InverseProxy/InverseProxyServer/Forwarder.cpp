#include "Forwarder.h"

#include <cassert>
#include <map>

#include <glib.h>

#include <CxxPtr/CPtr.h>

#include "RtspParser/Common.h"

#include "InverseProxyServerConfig.h"
#include "FrontSession.h"
#include "BackSession.h"

#include "Log.h"


static const auto Log = InverseProxyServerLog;

namespace {

struct BackSessionData {
    BackSession *const backSession;
    rtsp::Parameters list;
};

}

// FIXME! need some protection from different session on the same address
struct Forwarder::Private
{
    Private(const InverseProxyServerConfig&);

    const InverseProxyServerConfig& config;

    std::map<std::string, BackSessionData> backSessions;

    std::string cachedAllSourcesList;
};

Forwarder::Private::Private(const InverseProxyServerConfig& config) :
    config(config)
{
}


Forwarder::Forwarder(const InverseProxyServerConfig& config) :
    _p(std::make_unique<Private>(config))
{
}

Forwarder::~Forwarder()
{
}

const InverseProxyServerConfig& Forwarder::config() const
{
    return _p->config;
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
    const AuthTokens& backAuthTokens = config().backAuthTokens;

    const auto it = backAuthTokens.find(name);
    if(backAuthTokens.end() == it) {
        Log()->error("[Forwarder] Unknown streaming source \"{}\".", name);
        return false;
    }
    if(it->second != token) {
        Log()->error("[Forwarder] Invalid token for streaming source \"{}\".", name);
        return false;
    }

    const auto pair = _p->backSessions.emplace(name, BackSessionData{ session });
    if(!pair.second) {
        Log()->error("[Forwarder] Streaming source \"{}\" already registered.", name);
        return false;
    }

    Log()->info("[Forwarder] Streaming source \"{}\" registered.", name);

    return true;
}

bool Forwarder::swapBackSessionSourcesList(
    const std::string& name,
    rtsp::Parameters* sourcesList)
{
    assert(sourcesList);

    const auto backSessionIt = _p->backSessions.find(name);
    if(backSessionIt == _p->backSessions.end())
        return false;

    BackSessionData& data = backSessionIt->second;
    if(!data.list.empty())
        return false;

    data.list.swap(*sourcesList);

    _p->cachedAllSourcesList.clear();

    return true;
}

const std::string& Forwarder::allSourcesList()
{
    if(!_p->cachedAllSourcesList.empty())
        return _p->cachedAllSourcesList;

    for(const auto& sessionPair: _p->backSessions) {
        const std::string& name = sessionPair.first;
        const BackSessionData& data = sessionPair.second;
        for(const auto& sourcePair: data.list) {
            const std::string& sourceName = name + "/" + sourcePair.first;
            const std::string& sourceDescription = sourcePair.first;
            CharPtr escapedNamePtr(
                g_uri_escape_string(sourceName.data(), "/", false));
            if(!escapedNamePtr) {
                static std::string emptyList;
                return emptyList; // insufficient memory?
            }

            _p->cachedAllSourcesList += escapedNamePtr.get();
            _p->cachedAllSourcesList += ": " + sourceDescription + "\r\n";
        }
    }

    if(_p->cachedAllSourcesList.empty())
        _p->cachedAllSourcesList = "\r\n";

    return _p->cachedAllSourcesList;
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
    std::string sourceName = requestPtr->uri;

    const std::string::size_type spliterPos = sourceName.find('/');
    if(spliterPos != std::string::npos)
        sourceName.resize(spliterPos);

    const auto backSessionIt = _p->backSessions.find(sourceName);
    if(backSessionIt == _p->backSessions.end())
        return false;

    BackSession* target2 = backSessionIt->second.backSession;

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
