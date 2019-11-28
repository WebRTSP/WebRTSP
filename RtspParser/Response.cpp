#include "Response.h"


namespace rtsp {

rtsp::SessionId ResponseSession(const rtsp::Response& response)
{
    auto it = response.headerFields.find("session");
    if(response.headerFields.end() == it)
        return rtsp::SessionId();

    return it->second;
}

}
