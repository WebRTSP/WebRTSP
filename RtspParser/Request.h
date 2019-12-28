#pragma once

#include <string>
#include <map>

#include "Common.h"
#include "Methods.h"
#include "Protocols.h"


namespace rtsp {

struct Request {
    Method method;
    std::string uri;
    Protocol protocol;
    CSeq cseq;

    std::map<std::string, std::string> headerFields;
    std::string body;
};

SessionId RequestSession(const Request& request);
void SetRequestSession(Request*, const SessionId&);
std::string RequestContentType(const Request& request);

}
