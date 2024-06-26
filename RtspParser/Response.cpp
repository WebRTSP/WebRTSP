#include "Response.h"


namespace rtsp {

MediaSessionId ResponseSession(const Response& response)
{
    auto it = response.headerFields.find("session");
    if(response.headerFields.end() == it)
        return MediaSessionId();

    return it->second;
}

void SetResponseSession(Response* response, const MediaSessionId& session)
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

void SetContentType(Response* response, const std::string& contentType)
{
    response->headerFields.emplace(ContentTypeFieldName, contentType);
}

}
