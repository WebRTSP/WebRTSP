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

}

bool ClientSession::handleOptionsResponse(
    const rtsp::Response& response)
{
    return true;
}

bool ClientSession::handleResponse(
    const rtsp::Response& response)
{
    return false;
}

}
