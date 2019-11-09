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

void ClientSession::requestOptions()
{
    rtsp::Request request;
    request.method = rtsp::Method::OPTIONS;
    request.uri = "*";
    request.protocol = rtsp::Protocol::RTSP_1_0;
    request.cseq = _nextCSeq++;

    sendRequest(request);
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
