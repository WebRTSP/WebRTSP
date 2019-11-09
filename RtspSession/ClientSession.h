#pragma once

#include <functional>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"


namespace rtsp {

struct ClientSession
{
    ClientSession(const std::function<void (const rtsp::Request*)>&);

    virtual void onConnected() {}

    bool handleResponse(const rtsp::Response&);

protected:
    void requestOptions(const std::string& uri);

private:
    void sendRequest(const rtsp::Request&);

    bool handleOptionsResponse(const rtsp::Response&);

private:
    std::function<void (const rtsp::Request*)> _requestCallback;

    CSeq _nextCSeq = 1;
};

}
