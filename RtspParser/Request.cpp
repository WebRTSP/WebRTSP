#include "Request.h"


namespace rtsp {

MediaSessionId RequestSession(const Request& request)
{
    auto it = request.headerFields.find("session");
    if(request.headerFields.end() == it)
        return MediaSessionId();

    return it->second;
}

void SetRequestSession(Request* request, const MediaSessionId& session)
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

void SetContentType(Request* request, const std::string& contentType)
{
    request->headerFields.emplace(ContentTypeFieldName, contentType);
}

}
