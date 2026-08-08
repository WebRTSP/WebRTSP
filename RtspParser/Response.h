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
    MediaSessionId session;

    std::map<std::string, std::string, LessNoCase> headerFields;
    std::string contentType;
    std::string body;
};

}
