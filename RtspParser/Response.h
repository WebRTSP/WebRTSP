#pragma once

#include <string>
#include <map>

#include "Common.h"
#include "Methods.h"
#include "Protocols.h"
#include "LessNoCase.h"


namespace rtsp {

struct Response {
    Protocol protocol;
    unsigned statusCode;
    std::string reasonPhrase;
    CSeq cseq;

    std::map<std::string, std::string, LessNoCase> headerFields;
    std::string body;
};

MediaSessionId ResponseSession(const Response&);
void SetResponseSession(Response*, const MediaSessionId&);
std::string ResponseContentType(const Response&);
void SetContentType(Response*, const std::string&);

}
