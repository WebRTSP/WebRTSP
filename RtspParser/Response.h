#pragma once

#include <string>
#include <map>

#include "Methods.h"
#include "Protocols.h"


namespace rtsp {

struct Response {
    Protocol protocol;
    unsigned statusCode;
    std::string reasonPhrase;

    std::map<std::string, std::string> headerFields;
    std::string body;
};

}
