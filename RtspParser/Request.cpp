#include "Request.h"


namespace rtsp {

rtsp::SessionId RequestSession(const rtsp::Request& request)
{
    auto it = request.headerFields.find("session");
    if(request.headerFields.end() == it)
        return rtsp::SessionId();

    return it->second;
}

std::string RequestContentType(const rtsp::Request& request)
{
    auto it = request.headerFields.find("content-type");
    if(request.headerFields.end() == it)
        return std::string();

    return it->second;
}

}
