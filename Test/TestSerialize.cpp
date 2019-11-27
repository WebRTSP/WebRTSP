#include "TestSerialize.h"

#include <cassert>

#include "RtspParser/RtspSerialize.h"


void TestSerialize() noexcept
{
    rtsp::Request request;
    request.method = rtsp::Method::OPTIONS;
    request.uri = "*";
    request.protocol = rtsp::Protocol::WEBRTSP_0_1;
    request.cseq = 1;

    const std::string requestMessage = rtsp::Serialize(request);

    assert(requestMessage ==
        "OPTIONS * WEBRTSP/0.1\r\n"
        "CSeq: 1\r\n");

    rtsp::Response response;
    response.protocol = rtsp::Protocol::WEBRTSP_0_1;
    response.cseq = 1;
    response.headerFields.emplace("Public", "DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE");
    response.statusCode = 200;
    response.reasonPhrase = "OK";

    const std::string responseMessage = rtsp::Serialize(response);

    assert(responseMessage ==
        "WEBRTSP/0.1 200 OK\r\n"
        "CSeq: 1\r\n"
        "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n");

}
