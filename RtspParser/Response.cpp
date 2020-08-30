#include "Response.h"


namespace rtsp {

SessionId ResponseSession(const Response& response)
{
    auto it = response.headerFields.find("session");
    if(response.headerFields.end() == it)
        return SessionId();

    return it->second;
}

void SetResponseSession(Response* response, const SessionId& session)
{
    response->headerFields["session"] = session;
}

std::string ResponseContentType(const Response& response)
{
    auto it = response.headerFields.find("content-type");
    if(response.headerFields.end() == it)
        return std::string();

    return it->second;
}

}
