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

rtsp::SessionId ResponseSession(const rtsp::Response&);

}
