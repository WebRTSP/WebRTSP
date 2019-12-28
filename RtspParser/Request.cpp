#include "Request.h"


namespace rtsp {

SessionId RequestSession(const Request& request)
{
    auto it = request.headerFields.find("session");
    if(request.headerFields.end() == it)
        return SessionId();

    return it->second;
}

void SetRequestSession(Request* request, const SessionId& session)
{
    request->headerFields["session"] = session;
}

std::string RequestContentType(const Request& request)
{
    auto it = request.headerFields.find("content-type");
    if(request.headerFields.end() == it)
        return std::string();

    return it->second;
}

}
