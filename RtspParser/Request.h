#pragma once

#include <string>
#include <map>

#include "Common.h"
#include "Methods.h"
#include "Protocols.h"
#include "LessNoCase.h"


namespace rtsp {

struct Request {
    Method method;
    std::string uri;
    Protocol protocol = Protocol::WEBRTSP_0_2;
    CSeq cseq;

    std::map<std::string, std::string, LessNoCase> headerFields;
    std::string body;
};

MediaSessionId RequestSession(const Request& request);
void SetRequestSession(Request*, const MediaSessionId&);
std::string RequestContentType(const Request& request);
void SetContentType(Request*, const std::string&);
void SetBearerAuthorization(Request*, const std::string& token);

}
