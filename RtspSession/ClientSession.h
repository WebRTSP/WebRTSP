#pragma once

#include <functional>
#include <map>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"


namespace rtsp {

struct ClientSession
{
    ClientSession(const std::function<void (const rtsp::Request*)>&) noexcept;

    virtual void onConnected() noexcept {}

    bool handleResponse(const rtsp::Response&) noexcept;

protected:
    CSeq requestOptions(const std::string& uri) noexcept;
    CSeq requestDescribe(const std::string& uri) noexcept;
    CSeq requestSetup(const std::string& uri, const std::string& sdp) noexcept;
    CSeq requestPlay(const std::string& uri, const std::string& session) noexcept;
    CSeq requestTeardown(const std::string& uri, const std::string& session) noexcept;

    virtual bool onOptionsResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept
        { return false; }
    virtual bool onDescribeResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept
        { return false; }
    virtual bool onSetupResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept
        { return false; }
    virtual bool onPlayResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept
        { return false; }
    virtual bool onTeardownResponse(
        const rtsp::Request&, const rtsp::Response&) noexcept
        { return false; }

private:
    rtsp::Request* createRequest(
        rtsp::Method,
        const std::string& uri) noexcept;
    rtsp::Request* createRequest(
        rtsp::Method,
        const std::string& uri,
        const std::string& session) noexcept;

    void sendRequest(const rtsp::Request&) noexcept;

private:
    std::function<void (const rtsp::Request*)> _requestCallback;

    CSeq _nextCSeq = 1;

    std::map<CSeq, rtsp::Request> _sentRequests;
};

}
