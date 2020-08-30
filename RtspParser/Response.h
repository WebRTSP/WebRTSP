#pragma once

#include <string>
#include <map>

#include "Common.h"
#include "Methods.h"
#include "Protocols.h"


namespace rtsp {

struct Response {
    Protocol protocol;
    unsigned statusCode;
    std::string reasonPhrase;
    CSeq cseq;

    std::map<std::string, std::string> headerFields;
    std::string body;
};

SessionId ResponseSession(const Response&);
void SetResponseSession(Response*, const SessionId&);
std::string ResponseContentType(const Response& request);

}
