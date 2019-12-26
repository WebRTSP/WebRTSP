#include "ForwardContext.h"

#include <cassert>
#include <map>

#include "RtspParser/Common.h"

#include "FrontSession.h"
#include "BackSession.h"


namespace {

struct BackRequestInfo
{
    BackSession* source;
    rtsp::CSeq sourceCSeq;
};

struct FrontSessionData
{
    std::map<rtsp::CSeq, BackRequestInfo> requests;
};

struct FrontRequestInfo
{
    FrontSession* source;
    rtsp::CSeq sourceCSeq;
};

struct BackSessionData
{
    std::map<rtsp::CSeq, FrontRequestInfo> requests;
    std::map<rtsp::SessionId, FrontSession*> mediaSessions;
};

}

// FIXME! need some protection from different session on the same address
struct ForwardContext::Private
{
    std::map<std::string, BackSession*> backSessions;

    std::map<FrontSession*, FrontSessionData> frontSessionsData;
    std::map<BackSession*, BackSessionData> backSessionsData;

    bool registerMediaSession(
        FrontSession* requestSource,
        BackSessionData* sessionData,
        const rtsp::Request& request,
        const rtsp::Response& response);
    bool unregisterMediaSession(
        FrontSession* requestSource,
        BackSessionData* sessionData,
        const rtsp::Request& request,
        const rtsp::Response& response);

};

bool ForwardContext::Private::registerMediaSession(
    FrontSession* requestSource,
    BackSessionData* sessionData,
    const rtsp::Request& request,
    const rtsp::Response& response)
{
    const rtsp::SessionId sessionId = ResponseSession(response);
    const bool inserted =
        sessionData->mediaSessions.emplace(sessionId, requestSource).second;

    return inserted;
}

bool ForwardContext::Private::unregisterMediaSession(
    FrontSession* requestSource,
    BackSessionData* sessionData,
    const rtsp::Request& request,
    const rtsp::Response& response)
{
    const rtsp::SessionId sessionId = ResponseSession(response);

    const auto it =
        sessionData->mediaSessions.find(sessionId);
    if(sessionData->mediaSessions.end() == it)
        return false;

    sessionData->mediaSessions.erase(it);

    return true;
}


ForwardContext::ForwardContext() :
    _p(std::make_unique<Private>())
{
}

ForwardContext::~ForwardContext()
{
}

void ForwardContext::registerFrontSession(FrontSession* frontSession)
{
    _p->frontSessionsData.emplace(frontSession, FrontSessionData {});
}

void ForwardContext::removeFrontSession(FrontSession* frontSession)
{
    // FIXME! teardown opened media sessions on back sessions
    _p->frontSessionsData.erase(frontSession);
}

bool ForwardContext::registerBackSession(
    const std::string& name,
    BackSession* session)
{
    const auto pair = _p->backSessions.emplace(name, session);
    if(!pair.second)
        return false;

    _p->backSessionsData.emplace(session, BackSessionData{});

    return true;
}

void ForwardContext::removeBackSession(
    const std::string& name,
    BackSession* session)
{
    _p->backSessions.erase(name);
    _p->backSessionsData.erase(session);
}

bool ForwardContext::forwardToBackSession(
    FrontSession* source,
    std::unique_ptr<rtsp::Request>& requestPtr)
{
    const auto backSessionIt = _p->backSessions.find(requestPtr->uri);
    if(backSessionIt == _p->backSessions.end())
        return false;

    BackSession* backSession = backSessionIt->second;

    const auto backSessionDataIt = _p->backSessionsData.find(backSession);
    if(backSessionDataIt == _p->backSessionsData.end())
        return false;

    BackSessionData& backSessionData = backSessionDataIt->second;

    const rtsp::CSeq frontCSeq = requestPtr->cseq;
    const rtsp::CSeq backCSeq = backSession->forward(requestPtr);

    const bool inserted =
        backSessionData.requests.emplace(
            backCSeq,
            FrontRequestInfo { source, frontCSeq }).second;

    return inserted;
}

bool ForwardContext::forwardToBackSession(
    FrontSession* responseSource,
    const rtsp::Request& request,
    const rtsp::Response& response)
{
    const auto it = _p->frontSessionsData.find(responseSource);
    if(it == _p->frontSessionsData.end()) {
        // FIXME! how is it possible?!
        return false;
    }

    FrontSessionData& sessionData = it->second;
    const auto requestsIt = sessionData.requests.find(response.cseq);
    if(requestsIt == sessionData.requests.end()) {
        // FIXME! again, how is it possible?!
        return false;
    }

    const BackRequestInfo& requestInfo = requestsIt->second;

    if(_p->backSessionsData.find(requestInfo.source) == _p->backSessionsData.end()) {
        // session was dropped
        return false;
    }

    rtsp::Response tmpResponse = response;
    tmpResponse.cseq = requestInfo.sourceCSeq;
    requestInfo.source->forward(request, response);

    return true;
}

bool ForwardContext::forwardToFrontSession(
    BackSession* requestSource,
    const rtsp::Request& request)
{
    const auto it = _p->backSessionsData.find(requestSource);
    if(it == _p->backSessionsData.end()) {
        // FIXME! how is it possible?!
        return false;
    }

    BackSessionData& sessionData = it->second;

    const rtsp::SessionId sessionId = RequestSession(request);

    const auto mediaSessionIt = sessionData.mediaSessions.find(sessionId);
    if(sessionData.mediaSessions.end() == mediaSessionIt)
        return false;

    FrontSession* frontSession = mediaSessionIt->second;

    const auto frontSessionDataIt = _p->frontSessionsData.find(frontSession);
    if(frontSessionDataIt == _p->frontSessionsData.end())
        return false;

    FrontSessionData& frontSessionData = frontSessionDataIt->second;

    const rtsp::CSeq backCSeq = request.cseq;
    const rtsp::CSeq frontCSeq = frontSession->forward(request);

    const bool inserted =
        frontSessionData.requests.emplace(
            frontCSeq,
            BackRequestInfo { requestSource, backCSeq }).second;

    return inserted;
}

bool ForwardContext::forwardToFrontSession(
    BackSession* responseSource,
    const rtsp::Request& request,
    const rtsp::Response& response)
{
    const auto it = _p->backSessionsData.find(responseSource);
    if(it == _p->backSessionsData.end()) {
        // FIXME! how is it possible?!
        return false;
    }

    BackSessionData& sessionData = it->second;
    const auto requestsIt = sessionData.requests.find(response.cseq);
    if(requestsIt == sessionData.requests.end()) {
        // FIXME! again, how is it possible?!
        return false;
    }

    const FrontRequestInfo& requestInfo = requestsIt->second;
    FrontSession* requestSource = requestInfo.source;

    if(_p->frontSessionsData.find(requestInfo.source) == _p->frontSessionsData.end()) {
        // session was dropped
        return false;
    }

    if(rtsp::StatusCode::OK == response.statusCode) {
        if(rtsp::Method::DESCRIBE == request.method) {
            if(!_p->registerMediaSession(requestSource, &sessionData, request, response))
                return false;
        } else if(rtsp::Method::TEARDOWN == request.method) {
            if(!_p->unregisterMediaSession(requestSource, &sessionData, request, response))
                return false;
        }
    }

    rtsp::Response tmpResponse = response;
    tmpResponse.cseq = requestInfo.sourceCSeq;
    requestInfo.source->forward(request, response);

    return true;
}
