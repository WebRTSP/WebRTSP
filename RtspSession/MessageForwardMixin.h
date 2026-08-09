#pragma once

#include <map>
#include <unordered_map>

#include "RtspSession/Session.h"


namespace rtsp {

class MessageForwardMixin
{
    MessageForwardMixin(const MessageForwardMixin&) noexcept = delete;
    MessageForwardMixin(MessageForwardMixin&&) noexcept = delete;
    MessageForwardMixin& operator = (const MessageForwardMixin&) noexcept = delete;
    MessageForwardMixin& operator = (MessageForwardMixin&&) noexcept = delete;

protected:
    enum class SessionType {
        Regular,
        Agent,
    };

    MessageForwardMixin(SessionType, rtsp::Session*) noexcept;
    ~MessageForwardMixin() noexcept;

    // proxy session side
    bool forwardRequest(
        std::unique_ptr<rtsp::Request>&& requestPtr,
        std::string&& targetUri,
        MessageForwardMixin* requestTarget) noexcept;
    bool forwardMediaSessionRequest(
        std::unique_ptr<rtsp::Request>&&) noexcept;

    std::optional<bool> tryForwardResponse(
        const rtsp::Request&,
        std::unique_ptr<rtsp::Response>&&) noexcept;

private:
    class SessionHandle;

    struct ForwardedRequestData {
        std::string uri;
        rtsp::CSeq cseq;
        rtsp::MediaSessionId mediaSession;
        std::weak_ptr<SessionHandle> requestSource;
        rtsp::MediaSessionId ownMediaSession; // for response validation
    };

    struct MediaSessionInfo {
        std::weak_ptr<SessionHandle> mediaSessionOwner;
        std::string ownerSideUri;
        rtsp::MediaSessionId ownerSideMediaSession;
        std::string ownUri; // for requests validation
    };

    rtsp::Session* session() const { return _session; }

    const std::shared_ptr<SessionHandle>& sessionHandle() const noexcept
        { return _handle; }

    const rtsp::MediaSessionId& registerAgentMediaSession(
        const std::string& uri,
        const std::shared_ptr<SessionHandle>& mediaSessionOwner,
        const std::string& agentUri,
        const rtsp::MediaSessionId& agentMediaSession) noexcept;

    bool responseToOrphanedRequest(std::unique_ptr<rtsp::Request>&&) noexcept;

    void forwardTeardown(
        const rtsp::MediaSessionId& ownMediaSession,
        MediaSessionInfo&&) noexcept;

    bool sendForwardedRequest(
        const std::shared_ptr<SessionHandle>& requestSource,
        std::string&& sourceUri,
        rtsp::CSeq sourceCSeq,
        rtsp::MediaSessionId&& sourceMediaSession,
        std::unique_ptr<rtsp::Request>&&) noexcept;
    void sendForwardedResponse(
        std::unique_ptr<rtsp::Response>&&) noexcept;

private:
    const SessionType _sessionType; // some requests are forbidden for Agent sessions
    rtsp::Session *const _session;

    std::shared_ptr<SessionHandle> _handle;

    std::map<rtsp::CSeq, ForwardedRequestData> _forwardedRequests;
    std::map<rtsp::MediaSessionId, MediaSessionInfo> _ownMediaSession2foreignMediaSession;
};

}
