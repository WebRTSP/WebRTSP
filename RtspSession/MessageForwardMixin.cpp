#include "MessageForwardMixin.h"

#include <glib.h>

#include "RtspParser/RtspParser.h"
#include "RtspParser/RtspSerialize.h"
#include "Helpers/TurnRestApi.h"

#include "Log.h"


#define SESSION "[{}]" " "
#define TAG "[MessageForwardMixin]" " "


using namespace rtsp;

class MessageForwardMixin::SessionHandle
{
public:
    SessionHandle(MessageForwardMixin* owner) noexcept : _owner(owner) {}

    MessageForwardMixin* operator -> () noexcept { return _owner; }
    const MessageForwardMixin* operator -> () const noexcept { return _owner; }

private:
    MessageForwardMixin *const _owner;
};

MessageForwardMixin::MessageForwardMixin(SessionType type, Session* session) noexcept :
    _sessionType(type),
    _session(session),
    _handle(std::make_shared<SessionHandle>(this))
{
}

MessageForwardMixin::~MessageForwardMixin() noexcept
{
    for(auto& pair: _forwardedRequests) {
        const ForwardedRequestData& data = pair.second;
        const std::shared_ptr<SessionHandle> responseTarget = data.requestSource.lock();
        if(responseTarget) {
            if(data.mediaSession.empty()) {
                (*responseTarget)->session()->sendNotFoundResponse(data.cseq);
            } else {
                (*responseTarget)->session()->sendSessionNotFoundResponse(
                    data.cseq,
                    data.mediaSession);
            }
        }
    }
    _forwardedRequests.clear();

    for(auto& pair: _ownMediaSession2foreignMediaSession) {
        forwardTeardown(pair.first, std::move(pair.second));
    }
    _ownMediaSession2foreignMediaSession.clear();
}

const rtsp::MediaSessionId& MessageForwardMixin::registerAgentMediaSession(
    const std::string& uri,
    const std::shared_ptr<SessionHandle>& agentSession,
    const std::string& agentUri,
    const rtsp::MediaSessionId& agentMediaSession) noexcept
{
    auto [it, added] = _ownMediaSession2foreignMediaSession.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(std::move(session()->nextMediaSession())),
        std::forward_as_tuple(
            agentSession,
            agentUri,
            agentMediaSession,
            uri));
    assert(added);

    return it->first;
}

void MessageForwardMixin::forwardTeardown(
    const rtsp::MediaSessionId& ownMediaSession,
    MediaSessionInfo&& targetMediaSession) noexcept
{
    const std::shared_ptr<SessionHandle> teardownTarget =
        targetMediaSession.mediaSessionOwner.lock();
    if(!teardownTarget) // already dead session
        return;

    std::unique_ptr<rtsp::Request> requestPtr =
        std::make_unique<rtsp::Request>();
    requestPtr->method = rtsp::Method::TEARDOWN;
    requestPtr->uri = targetMediaSession.ownerSideUri;
    requestPtr->session = targetMediaSession.ownerSideMediaSession;
    (*teardownTarget)->sendForwardedRequest(
        _handle,
        std::move(targetMediaSession.ownUri),
        rtsp::CSeq(), // no response required
        std::string(ownMediaSession),
        std::move(requestPtr));
}

bool MessageForwardMixin::forwardRequest(
    std::unique_ptr<rtsp::Request>&& requestPtr,
    std::string&& targetUri,
    MessageForwardMixin* requestTarget) noexcept
{
    assert(_sessionType == SessionType::Regular);

    rtsp::MediaSessionId mediaSession = std::move(requestPtr->session);

    switch(requestPtr->method) {
        case rtsp::Method::OPTIONS:
        case rtsp::Method::LIST:
        case rtsp::Method::DESCRIBE:
            if(!mediaSession.empty()) {
                session()->log()->error(
                    SESSION TAG "Unexpected media session in {} request",
                    session()->sessionLogId,
                    MethodName(requestPtr->method));
                return false;
            }
            break;
        default:
            if(mediaSession.empty()) {
                session()->log()->error(
                    SESSION TAG "Media session is missing in request",
                    session()->sessionLogId);
                return false;
            }
            break;
    }

    if(!mediaSession.empty()) {
        auto it = _ownMediaSession2foreignMediaSession.find(mediaSession);
        if(it == _ownMediaSession2foreignMediaSession.end()) {
            session()->log()->error(
                SESSION TAG "Can't find target media session",
                session()->sessionLogId);
            return false;
        }

        const MediaSessionInfo& info = it->second;

        if(requestPtr->uri != info.ownUri || targetUri != info.ownerSideUri) {
            session()->log()->error(
                SESSION TAG "Requested URI doesn't match to the URI of the media session",
                session()->sessionLogId);
            return false;
        }

        requestPtr->session = info.ownerSideMediaSession;
    }

    std::string uri = std::move(requestPtr->uri);
    const rtsp::CSeq cseq = requestPtr->cseq;

    requestPtr->uri = std::move(targetUri);
    requestPtr->cseq = rtsp::InvalidCSeq;

    return requestTarget->sendForwardedRequest(
        sessionHandle(),
        std::move(uri),
        cseq,
        std::move(mediaSession),
        std::move(requestPtr));
}

bool MessageForwardMixin::forwardMediaSessionRequest(
    std::unique_ptr<rtsp::Request>&& requestPtr) noexcept
{
    assert(_sessionType == SessionType::Regular || _sessionType == SessionType::Agent);

    if(requestPtr->session.empty()) {
        session()->log()->error(
            SESSION TAG "Missing media session in request",
            session()->sessionLogId);
        return false;
    }

    auto it = _ownMediaSession2foreignMediaSession.find(requestPtr->session);
    if(it == _ownMediaSession2foreignMediaSession.end()) {
        session()->log()->error(
            SESSION TAG "Unknown media session in request",
            session()->sessionLogId);
        return false;
    }

    rtsp::MediaSessionId mediaSession = std::move(requestPtr->session);

    if(_sessionType == SessionType::Agent) {
        switch(requestPtr->method) {
            case rtsp::Method::OPTIONS:
            case rtsp::Method::DESCRIBE:
            case rtsp::Method::PLAY:
            case rtsp::Method::SUBSCRIBE:
                session()->log()->error(
                    SESSION TAG "Got unsupported request type {}",
                    session()->sessionLogId,
                    MethodName(requestPtr->method));
                return false;
        }
    }

    const MediaSessionInfo& info = it->second;

    const std::shared_ptr<SessionHandle> requestTarget = info.mediaSessionOwner.lock();
    if(!requestTarget) {
        session()->log()->warn(
            SESSION TAG "Target session is missing",
            session()->sessionLogId);
        return responseToOrphanedRequest(std::move(requestPtr));
    }

    if(requestPtr->uri != info.ownUri) {
        session()->log()->error(
            SESSION TAG "Requested URI doesn't match to the URI of the media session",
            session()->sessionLogId);
        return false;
    }

    std::string uri = std::move(requestPtr->uri);
    const rtsp::CSeq cseq = requestPtr->cseq;

    requestPtr->session = info.ownerSideMediaSession;
    requestPtr->uri = info.ownerSideUri;
    requestPtr->cseq = rtsp::InvalidCSeq;

    return (*requestTarget)->sendForwardedRequest(
        sessionHandle(),
        std::move(uri),
        cseq,
        std::move(mediaSession),
        std::move(requestPtr));
}

bool MessageForwardMixin::responseToOrphanedRequest(
    std::unique_ptr<rtsp::Request>&& requestPtr) noexcept
{
    if(requestPtr->method == rtsp::Method::TEARDOWN) {
        session()->sendOkResponse(requestPtr->cseq);
    } else {
        session()->sendSessionNotFoundResponse(
            requestPtr->cseq,
            requestPtr->session);
    }

    return true;
}

std::optional<bool> MessageForwardMixin::tryForwardResponse(
    const rtsp::Request& request,
    std::unique_ptr<rtsp::Response>&& responsePtr) noexcept
{
    auto it = _forwardedRequests.find(request.cseq);
    if(it == _forwardedRequests.end())
        return {};

    ForwardedRequestData forwardedRequest = std::move(it->second);
    _forwardedRequests.erase(it);

    if(forwardedRequest.cseq == rtsp::CSeq()) // source session doesn't need answer
        return true;

    const std::shared_ptr<SessionHandle> requestSource = forwardedRequest.requestSource.lock();
    if(!requestSource)
        return true; // already dead session

    const rtsp::MediaSessionId mediaSession = responsePtr->session;
    rtsp::MediaSessionId targetMediaSession;

    bool badGateway = false;
    if(request.method == rtsp::Method::DESCRIBE) {
        assert(forwardedRequest.mediaSession.empty());
        if(responsePtr->statusCode == rtsp::StatusCode::OK) {
            if(mediaSession.empty()) {
                session()->log()->error(
                    SESSION TAG "Got response to DESCRIBE without media sesssion",
                    session()->sessionLogId);
                badGateway = true;
           } else {
                rtsp::MediaSessionId proxyMediaSession =
                    (*requestSource)->registerAgentMediaSession(
                        forwardedRequest.uri,
                        _handle,
                        request.uri,
                        mediaSession);
                _ownMediaSession2foreignMediaSession.emplace(
                    mediaSession,
                    MediaSessionInfo {
                        requestSource,
                        forwardedRequest.uri,
                        proxyMediaSession,
                        request.uri }); // own Uri 

                targetMediaSession = std::move(proxyMediaSession);
            }
        }
    } else if(request.method == rtsp::Method::TEARDOWN) {
        assert(!forwardedRequest.mediaSession.empty()); // BUG!
        _ownMediaSession2foreignMediaSession.erase(forwardedRequest.mediaSession);
    } else if(mediaSession.empty()) {
        switch(request.method) {
        case rtsp::Method::OPTIONS:
        case rtsp::Method::LIST:
            break;
        default:
            session()->log()->error(
                SESSION TAG "Got response without media session",
                session()->sessionLogId);
            badGateway = true;
            break;
        }
    } else {
        if(forwardedRequest.ownMediaSession != mediaSession) {
            session()->log()->error(
                SESSION TAG "Got response with mismatched media session",
                session()->sessionLogId);
            badGateway = true;
        } else {
            auto mediaSessionIt = _ownMediaSession2foreignMediaSession.find(mediaSession);
            assert(
                mediaSessionIt != _ownMediaSession2foreignMediaSession.end() &&
                mediaSessionIt->second.ownerSideMediaSession == forwardedRequest.mediaSession); // BUG!

            targetMediaSession = std::move(forwardedRequest.mediaSession);
        }
    }

    if(mediaSession.empty() && targetMediaSession.empty()) {
        session()->log()->debug(
            "Forwarding {} response:\n"
            "[{}] -> [{}]\n"
            "Uri: \"{}\" -> \"{}\"\n"
            "CSeq: {} -> {}",
            MethodName(request.method),
            session()->sessionLogId, (*requestSource)->session()->sessionLogId,
            request.uri, forwardedRequest.uri,
            responsePtr->cseq, forwardedRequest.cseq);
    } else {
        session()->log()->debug(
            "Forwarding {} response:\n"
            "[{}] -> [{}]\n"
            "Uri: \"{}\" -> \"{}\"\n"
            "CSeq: {} -> {}\n"
            "Session: \"{}\" -> \"{}\"",
            MethodName(request.method),
            session()->sessionLogId, (*requestSource)->session()->sessionLogId,
            request.uri, forwardedRequest.uri,
            responsePtr->cseq, forwardedRequest.cseq,
            mediaSession, targetMediaSession);
    }

    if(badGateway) {
        const rtsp::CSeq cseq = responsePtr->cseq;
        responsePtr = std::make_unique<rtsp::Response>();
        rtsp::Session::prepareBadGatewayResponse(cseq, {}, responsePtr.get());
    } else {
        responsePtr->cseq = forwardedRequest.cseq;
        responsePtr->session = targetMediaSession;
    }

    (*requestSource)->sendForwardedResponse(std::move(responsePtr));

    return !badGateway;
}

bool MessageForwardMixin::sendForwardedRequest(
    const std::shared_ptr<SessionHandle>& requestSource,
    std::string&& sourceUri,
    rtsp::CSeq sourceCSeq,
    rtsp::MediaSessionId&& sourceMediaSession,
    std::unique_ptr<rtsp::Request>&& requestPtr) noexcept
{
    rtsp::Request* attachedRequest = session()->attachRequest(std::move(requestPtr));

    switch(requestPtr->method) {
    case rtsp::Method::OPTIONS:
    case rtsp::Method::DESCRIBE:
        assert(sourceMediaSession.empty() && attachedRequest->session.empty());
        break;
    case rtsp::Method::SETUP:
    case rtsp::Method::PLAY:
    case rtsp::Method::TEARDOWN:
        assert(!sourceMediaSession.empty() && !attachedRequest->session.empty());
        break;
    }

    const auto [it, added] = _forwardedRequests.try_emplace(
        attachedRequest->cseq,
        std::move(sourceUri),
        sourceCSeq,
        std::move(sourceMediaSession),
        requestSource,
        attachedRequest->session);
    assert(added);

    if(it->second.mediaSession.empty() && attachedRequest->session.empty()) {
        session()->log()->debug(
            "Forwarding {} request:\n"
            "[{}] -> [{}]\n"
            "Uri: \"{}\" -> \"{}\"\n"
            "CSeq: {} -> {}",
            MethodName(attachedRequest->method),
            (*requestSource)->session()->sessionLogId, session()->sessionLogId,
            it->second.uri, attachedRequest->uri,
            sourceCSeq, attachedRequest->cseq);

    } else {
        session()->log()->debug(
            "Forwarding {} request:\n"
            "[{}] -> [{}]\n"
            "Uri: \"{}\" -> \"{}\"\n"
            "CSeq: {} -> {}\n"
            "Session: \"{}\" -> \"{}\"",
            MethodName(attachedRequest->method),
            (*requestSource)->session()->sessionLogId, session()->sessionLogId,
            it->second.uri, attachedRequest->uri,
            sourceCSeq, attachedRequest->cseq,
            it->second.mediaSession, attachedRequest->session);
    }

    session()->sendRequest(*attachedRequest);

    return true;
}

void MessageForwardMixin::sendForwardedResponse(
    std::unique_ptr<rtsp::Response>&& responsePtr) noexcept
{
    session()->sendResponse(*responsePtr);
}
