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
    request.headerFields.emplace("CSeq", std::to_string(_nextCSeq));
    ++_nextCSeq;

    sendRequest(request);
}

bool ClientSession::handleOptionsResponse(
    const rtsp::Response& response)
{
    auto it = response.headerFields.find("cseq");
    if(response.headerFields.end() == it || it->second.empty())
        return false;

    return true;
}

bool ClientSession::handleResponse(
    const rtsp::Response& response)
{
    return false;
}

}
