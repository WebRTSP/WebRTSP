#include "ClientSession.h"

#include <set>

#include "RtspParser/RtspParser.h"

#include "RtspSession/StatusCode.h"


namespace {

inline bool IsMethodSupported(
    const std::set<rtsp::Method>& supportedMethods,
    rtsp::Method method) noexcept
{
    return supportedMethods.find(method) != supportedMethods.end();
}

bool IsPlaySupported(const std::set<rtsp::Method>& supportedMethods) noexcept
{
    return
        IsMethodSupported(supportedMethods, rtsp::Method::DESCRIBE) &&
        IsMethodSupported(supportedMethods, rtsp::Method::SETUP) &&
        IsMethodSupported(supportedMethods, rtsp::Method::PLAY) &&
        IsMethodSupported(supportedMethods, rtsp::Method::TEARDOWN);
}

bool IsSubscribeSupported(const std::set<rtsp::Method>& supportedMethods) noexcept
{
    return
        IsMethodSupported(supportedMethods, rtsp::Method::SUBSCRIBE) &&
        IsMethodSupported(supportedMethods, rtsp::Method::SETUP) &&
        IsMethodSupported(supportedMethods, rtsp::Method::TEARDOWN);
}

}

struct ClientSession::Private
{
    Private(
        ClientSession* owner,
        const std::string& uri,
        const CreatePeer& createPeer);

    ClientSession* owner;

    std::string uri;

    std::unique_ptr<WebRTCPeer> receiver;
    rtsp::MediaSessionId session;
    rtsp::CSeq recordRequestCSeq = rtsp::InvalidCSeq;

    void receiverPrepared();
    void iceCandidate(unsigned, const std::string&);
    void eos();
};

ClientSession::Private::Private(
    ClientSession* owner,
    const std::string& uri,
    const CreatePeer& createPeer) :
    owner(owner), uri(uri), receiver(createPeer())
{
}

void ClientSession::Private::receiverPrepared()
{
    if(receiver->sdp().empty()) {
        owner->disconnect();
        return;
    }

    if(recordRequestCSeq != rtsp::InvalidCSeq) {
        owner->sendOkResponse(
            recordRequestCSeq,
            session,
            rtsp::SdpContentType,
            receiver->sdp());

        receiver->play();
        recordRequestCSeq = rtsp::InvalidCSeq;
    } else {
        owner->requestPlay(
            uri,
            session,
            receiver->sdp());
    }
}

void ClientSession::Private::iceCandidate(
    unsigned mlineIndex, const std::string& candidate)
{
    owner->requestSetup(
        uri,
        rtsp::IceCandidateContentType,
        session,
        std::to_string(mlineIndex) + "/" + candidate + "\r\n");
}

void ClientSession::Private::eos()
{
    owner->log()->trace("Eos");

    owner->onEos(); // FIXME! send TEARDOWN and remove Media Session instead
}


ClientSession::ClientSession(
    const std::string& uri,
    const WebRTCConfigPtr& webRTCConfig,
    const CreatePeer& createPeer,
    const SendRequest& sendRequest,
    const SendResponse& sendResponse) noexcept :
    rtsp::Session(webRTCConfig, sendRequest, sendResponse),
    _p(new Private(this, uri, createPeer))
{
}

ClientSession::ClientSession(
    const WebRTCConfigPtr& webRTCConfig,
    const CreatePeer& createPeer,
    const SendRequest& sendRequest,
    const SendResponse& sendResponse) noexcept :
    ClientSession(std::string(), webRTCConfig, createPeer, sendRequest, sendResponse)
{
}

ClientSession::~ClientSession()
{
}

void ClientSession::setUri(const std::string& uri)
{
    _p->uri = uri;
}

bool ClientSession::onConnected() noexcept
{
    if(!_p->uri.empty())
        requestOptions(_p->uri);

    return true;
}

rtsp::CSeq ClientSession::requestDescribe() noexcept
{
    assert(!_p->uri.empty());
    return rtsp::Session::requestDescribe(_p->uri);
}

rtsp::CSeq ClientSession::requestSubscribe() noexcept
{
    assert(!_p->uri.empty());
    return rtsp::Session::requestSubscribe(_p->uri);
}

bool ClientSession::onOptionsResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    const FeatureState playSupport = playSupportState(request.uri);
    const FeatureState subscribeSupport = subscribeSupportState(request.uri);

    const std::set<rtsp::Method> supportedMethods = rtsp::ParseOptions(response);

    if(playSupport == FeatureState::Required && !IsPlaySupported(supportedMethods))
        return false;

    if(subscribeSupport == FeatureState::Required && !IsSubscribeSupported(supportedMethods))
        return false;

    if(subscribeSupport != FeatureState::Disabled && IsSubscribeSupported(supportedMethods)) {
        requestSubscribe();
        return true;
    } else if(playSupport != FeatureState::Disabled && IsPlaySupported(supportedMethods)) {
        requestDescribe();
        return true;
    }

    return false;
}

bool ClientSession::onDescribeResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    assert(_p->session.empty());

    _p->session = ResponseSession(response);
    if(_p->session.empty())
        return false;

    const std::string& sdp = response.body;
    if(sdp.empty())
        return false;

    _p->receiver->prepare(
        webRTCConfig(),
        std::bind(
            &ClientSession::Private::receiverPrepared,
            _p.get()),
        std::bind(
            &ClientSession::Private::iceCandidate,
            _p.get(),
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(
            &ClientSession::Private::eos,
            _p.get()),
        sessionLogId);

    _p->receiver->setRemoteSdp(sdp);

    return true;
}

bool ClientSession::onSetupResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    if(ResponseSession(response) != _p->session)
        return false;

    return true;
}

bool ClientSession::onPlayResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    if(ResponseSession(response) != _p->session)
        return false;

    _p->receiver->play();

    return true;
}

bool ClientSession::onSubscribeResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(rtsp::StatusCode::OK != response.statusCode)
        return false;

    assert(_p->session.empty());

    _p->session = ResponseSession(response);
    if(_p->session.empty())
        return false;

    return true;
}

bool ClientSession::onTeardownResponse(
    const rtsp::Request& request,
    const rtsp::Response& response) noexcept
{
    if(ResponseSession(response) != _p->session)
        return false;

    return false;
}

bool ClientSession::onRecordRequest(std::unique_ptr<rtsp::Request>& request) noexcept
{
    rtsp::MediaSessionId recordSession = RequestSession(*request);
    if(_p->session != recordSession)
        return false;

    _p->recordRequestCSeq = request->cseq;

    const std::string& sdp = request->body;
    if(sdp.empty())
        return false;

    _p->receiver->prepare(
        webRTCConfig(),
        std::bind(
            &ClientSession::Private::receiverPrepared,
            _p.get()),
        std::bind(
            &ClientSession::Private::iceCandidate,
            _p.get(),
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(
            &ClientSession::Private::eos,
            _p.get()),
        sessionLogId);

    _p->receiver->setRemoteSdp(sdp);

    return true;

}

bool ClientSession::onSetupRequest(std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    if(RequestSession(*requestPtr) != _p->session)
        return false;

    if(RequestContentType(*requestPtr) != rtsp::IceCandidateContentType)
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

        try {
            const int idx = std::stoi(line.substr(0, delimiterPos));
            if(idx < 0)
                return false;

            const std::string candidate =
                line.substr(delimiterPos + 1);

            if(candidate.empty())
                return false;

            log()->trace("Adding ice candidate \"{}\"", candidate);

            _p->receiver->addIceCandidate(idx, candidate);
        } catch(...) {
            return false;
        }
        pos = lineEndPos + 2;
    }

    sendOkResponse(requestPtr->cseq, rtsp::RequestSession(*requestPtr));

    return true;
}
