#include "Session.h"


namespace rtsp {

Session::Session(
    const std::function<void (const Request*)>& sendRequest,
    const std::function<void (const Response*)>& sendResponse) noexcept :
    _sendRequest(sendRequest), _sendResponse(sendResponse)
{
}

Request* Session::createRequest(
    Method method,
    const std::string& uri) noexcept
{
    for(;;) {
        const auto& pair =
            _sentRequests.emplace(
                _nextCSeq,
                Request{
                    .method = method,
                    .protocol = Protocol::WEBRTSP_0_1,
                    .cseq = _nextCSeq
                });

        ++_nextCSeq;

        if(pair.second) {
            Request& request = pair.first->second;
            request.uri = uri;
            return &request;
        }
    }
}

Request* Session::createRequest(
    Method method,
    const std::string& uri,
    const SessionId& session) noexcept
{
    Request* request = createRequest(method, uri);

    request->headerFields.emplace("Session", session);

    return request;
}

Response* Session::prepareResponse(
    CSeq cseq,
    StatusCode statusCode,
    const std::string::value_type* reasonPhrase,
    const SessionId& session,
    Response* out)
{
    out->protocol = Protocol::WEBRTSP_0_1;
    out->cseq = cseq;
    out->statusCode = statusCode;
    out->reasonPhrase = reasonPhrase;

    if(!session.empty())
        out->headerFields.emplace("session", session);

    return out;
}

Response* Session::prepareOkResponse(
    CSeq cseq,
    const SessionId& session,
    Response* out)
{
    return prepareResponse(cseq, OK, "OK", session, out);
}

void Session::sendOkResponse(
    rtsp::CSeq cseq,
    const rtsp::SessionId& session)
{
    rtsp::Response response;
    sendResponse(*prepareOkResponse(cseq, session, &response));
}

void Session::sendRequest(const Request& request) noexcept
{
    _sendRequest(&request);
}

void Session::sendResponse(const Response& response) noexcept
{
    _sendResponse(&response);
}

void Session::disconnect() noexcept
{
    _sendRequest(nullptr);
}

bool Session::handleResponse(const Response& response) noexcept
{
    auto it = _sentRequests.find(response.cseq);
    if(it == _sentRequests.end())
        return false;

    const Request& request = it->second;
    return handleResponse(request, response);
}

}
