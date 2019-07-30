#pragma once

#include <string>
#include <map>

#include "Methods.h"
#include "Protocols.h"


namespace rtsp {

struct Request {
    Method method;
    std::string uri;
    Protocol protocol;

    std::map<std::string, std::string> headerFields;
    std::string body;
};

}
