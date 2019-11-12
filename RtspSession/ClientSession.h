#pragma once

#include <functional>
#include <map>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"


namespace rtsp {

struct ClientSession
{
    ClientSession(const std::function<void (const rtsp::Request*)>&);

    virtual void onConnected() {}

    bool handleResponse(const rtsp::Response&);

protected:
    CSeq requestOptions(const std::string& uri);
    CSeq requestDescribe(const std::string& uri);
    CSeq requestSetup(const std::string& uri);
    CSeq requestPlay(const std::string& uri, const std::string& session);
    CSeq requestTeardown(const std::string& uri, const std::string& session);

    virtual bool onOptionsResponse(
        const rtsp::Request&, const rtsp::Response&)
        { return false; }
    virtual bool onDescribeResponse(
        const rtsp::Request&, const rtsp::Response&)
        { return false; }
    virtual bool onSetupResponse(
        const rtsp::Request&, const rtsp::Response&)
        { return false; }
    virtual bool onPlayResponse(
        const rtsp::Request&, const rtsp::Response&)
        { return false; }
    virtual bool onTeardownResponse(
        const rtsp::Request&, const rtsp::Response&)
        { return false; }

private:
    rtsp::Request* createRequest(
        rtsp::Method,
        const std::string& uri);
    rtsp::Request* createRequest(
        rtsp::Method,
        const std::string& uri,
        const std::string& session);

    void sendRequest(const rtsp::Request&);

private:
    std::function<void (const rtsp::Request*)> _requestCallback;

    CSeq _nextCSeq = 1;

    std::map<CSeq, rtsp::Request> _sentRequests;
};

}
