#pragma once

#include <functional>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"


namespace rtsp {

struct ClientSession
{
    ClientSession(const std::function<void (const rtsp::Request*)>&);

    virtual void onConnected() {}

    virtual bool handleResponse(const rtsp::Response&)
        { return false; }

protected:
    CSeq requestOptions(const std::string& uri);
    CSeq requestDescribe(const std::string& uri);
    CSeq requestSetup(const std::string& uri);
    CSeq requestPlay(const std::string& uri, const std::string& session);
    CSeq requestTeardown(const std::string& uri, const std::string& session);

    virtual bool onResponse(const rtsp::Response&) { return false; }

private:
    void sendRequest(const rtsp::Request&);

    bool handleOptionsResponse(const rtsp::Response&);

private:
    std::function<void (const rtsp::Request*)> _requestCallback;

    CSeq _nextCSeq = 1;
};

}
