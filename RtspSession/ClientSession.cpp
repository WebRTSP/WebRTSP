#include "ClientSession.h"

namespace rtsp {

ClientSession::ClientSession(const std::function<void (const rtsp::Request*)>& cb) :
    _requestCallback(cb)
{
}

void ClientSession::sendRequest(const rtsp::Request& request)
{
    _requestCallback(&request);
}

CSeq ClientSession::requestOptions(const std::string& uri)
{
    rtsp::Request request;
    request.method = rtsp::Method::OPTIONS;
    request.uri = uri;
    request.protocol = rtsp::Protocol::RTSP_1_0;
    request.cseq = _nextCSeq++;

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestDescribe(const std::string& uri)
{
    rtsp::Request request;
    request.method = rtsp::Method::DESCRIBE;
    request.uri = uri;
    request.protocol = rtsp::Protocol::RTSP_1_0;
    request.cseq = _nextCSeq++;

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestSetup(const std::string& uri)
{
    rtsp::Request request;
    request.method = rtsp::Method::SETUP;
    request.uri = uri;
    request.protocol = rtsp::Protocol::RTSP_1_0;
    request.cseq = _nextCSeq++;

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestPlay(
    const std::string& uri,
    const std::string& session)
{
    rtsp::Request request;
    request.method = rtsp::Method::PLAY;
    request.uri = uri;
    request.protocol = rtsp::Protocol::RTSP_1_0;
    request.cseq = _nextCSeq++;

    request.headerFields.emplace("Session", session);

    sendRequest(request);

    return request.cseq;
}

CSeq ClientSession::requestTeardown(
    const std::string& uri,
    const std::string& session)
{
    rtsp::Request request;
    request.method = rtsp::Method::TEARDOWN;
    request.uri = uri;
    request.protocol = rtsp::Protocol::RTSP_1_0;
    request.cseq = _nextCSeq++;

    request.headerFields.emplace("Session", session);

    sendRequest(request);

    return request.cseq;
}

}
