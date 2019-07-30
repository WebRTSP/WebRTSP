#include "TestParseRequest.h"

#include <cassert>

#include "RtspParser/RtspParser.h"


void TestParse()
{
    {
        const char OPTIONSRequest[] =
            " OPTIONS * RTSP/1.0";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(!success);
    }

    {
        const char OPTIONSRequest[] =
            "OPTIONS * RTSP/1.0 ";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(!success);
    }

    {
        const char OPTIONSRequest[] =
            "OPTIONS*RTSP/1.0";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(!success);
    }

    {
        const char OPTIONSRequest[] =
            "OPTION * RTSP/1.0";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(!success);
    }

    {
        const char OPTIONSRequest[] =
            "OPTIONS RTS/1.0";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(!success);
    }

    {
        const char OPTIONSRequest[] =
            "OPTIONS * RTS/1.0";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(!success);
    }

    {
        const char OPTIONSRequest[] =
            "OPTIONS * RTSP/.0";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(!success);
    }

    {
        const char OPTIONSRequest[] =
            "OPTIONS * RTSP/1.0";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(!success);
    }

    {
        const char OPTIONSRequest[] =
            "OPTIONS * RTSP/1.0\r\n";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(success);
        assert(request.method == rtsp::Method::OPTIONS);
        assert(request.uri == "*");
        assert(request.protocol == rtsp::Protocol::RTSP_1_0);
    }

    {
        const char OPTIONSRequest[] =
            "OPTIONS * RTSP/1.0\r\n"
            "CSeq: 1";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(!success);
    }

    {
        const char OPTIONSRequest[] =
            "OPTIONS * RTSP/1.0\r\n"
            "CSeq: 1\r\n";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(success);
        assert(request.method == rtsp::Method::OPTIONS);
        assert(request.uri == "*");
        assert(request.protocol == rtsp::Protocol::RTSP_1_0);
        assert(
            request.headerFields.size() == 1 &&
            request.headerFields.begin()->first == "cseq" &&
            request.headerFields.begin()->second == "1");
    }
    {
        const char OPTIONSRequest[] =
            "OPTIONS * RTSP/1.0\r\n"
            "CSeq:       1\r\n";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(OPTIONSRequest, sizeof(OPTIONSRequest) - 1, &request);
        assert(success);
        assert(request.method == rtsp::Method::OPTIONS);
        assert(request.uri == "*");
        assert(request.protocol == rtsp::Protocol::RTSP_1_0);
        assert(
            request.headerFields.size() == 1 &&
            request.headerFields.begin()->first == "cseq" &&
            request.headerFields.begin()->second == "1");
    }

    {
        const char SETUPRequest[] =
            "SETUP rtsp://example.com/meida.ogg/streamid=0 RTSP/1.0\r\n"
            "CSeq: 3\r\n"
            "Transport: RTP/AVP;unicast;client_port=8000-8001\r\n";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(SETUPRequest, sizeof(SETUPRequest) - 1, &request);
        assert(success);
        assert(request.method == rtsp::Method::SETUP);
        assert(request.uri == "rtsp://example.com/meida.ogg/streamid=0");
        assert(request.protocol == rtsp::Protocol::RTSP_1_0);
        assert(request.headerFields.size() == 2);
        if(request.headerFields.size() == 2) {
            auto it = request.headerFields.begin();
            assert(
                it->first == "cseq" &&
                it->second == "3");

            ++it;
            assert(
                it->first == "transport" &&
                it->second == "RTP/AVP;unicast;client_port=8000-8001");
        }
    }

    {
        const char GET_PARAMETERRequest[] =
            "GET_PARAMETER rtsp://example.com/media.mp4 RTSP/1.0\r\n"
            "CSeq: 9\r\n"
            "Content-Type: text/parameters\r\n"
            "Session: 12345678\r\n"
            "Content-Length: 15\r\n"
            "\r\n"
            "packets_received\r\n"
            "jitter\r\n";
        rtsp::Request request;
        const bool success =
            rtsp::ParseRequest(GET_PARAMETERRequest, sizeof(GET_PARAMETERRequest) - 1, &request);
        assert(success);
        assert(request.method == rtsp::Method::GET_PARAMETER);
        assert(request.headerFields.size() == 4);
        assert(!request.body.empty());
    }

    {
        const char GET_PARAMETERResponse[] =
            "RTSP/1.0 200 OK\r\n"
            "CSeq: 9\r\n"
            "Content-Length: 46\r\n"
            "Content-Type: text/parameters\r\n"
            "\r\n"
            "packets_received: 10\r\n"
            "jitter: 0.3838\r\n";
        rtsp::Response response;
        const bool success =
            rtsp::ParseResponse(GET_PARAMETERResponse, sizeof(GET_PARAMETERResponse) - 1, &response);
        assert(success);
        assert(response.protocol == rtsp::Protocol::RTSP_1_0);
        assert(response.statusCode == 200);
        assert(response.reasonPhrase == "OK");
        assert(response.headerFields.size() == 3);
        assert(!response.body.empty());
    }
}
