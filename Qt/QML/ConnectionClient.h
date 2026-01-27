#pragma once

#include <QObject>
#include <QPointer>

#include "RtspParser/Request.h"
#include "RtspParser/Response.h"
#include "RtspSession/StatusCode.h"


namespace webrtsp::qml {

class Connection;
class ConnectionClient: public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(Connection* connection  READ connection)

protected:
    ConnectionClient(Connection* connection) noexcept;
    virtual ~ConnectionClient() noexcept;

    Connection* connection() const { return _connection; }

    virtual void onConnected() noexcept {}
    virtual void onDisconnected() noexcept {}

    bool handleRequest(std::unique_ptr<rtsp::Request>&) noexcept;
    bool handleResponse(
        const rtsp::Request&,
        std::unique_ptr<rtsp::Response>&) noexcept;

    virtual bool onOptionsRequest(std::unique_ptr<rtsp::Request>&) noexcept
        { return false; }
    virtual bool onListRequest(std::unique_ptr<rtsp::Request>&) noexcept
        { return false; }
    virtual bool onDescribeRequest(std::unique_ptr<rtsp::Request>&) noexcept
        { return false; }
    virtual bool onSetupRequest(std::unique_ptr<rtsp::Request>&) noexcept
        { return false; }
    virtual bool onPlayRequest(std::unique_ptr<rtsp::Request>&) noexcept
        { return false; }
    virtual bool onSubscribeRequest(std::unique_ptr<rtsp::Request>&) noexcept
        { return false; }
    virtual bool onRecordRequest(std::unique_ptr<rtsp::Request>&) noexcept
        { return false; }
    virtual bool onTeardownRequest(std::unique_ptr<rtsp::Request>&) noexcept
        { return false; }
    virtual bool onGetParameterRequest(std::unique_ptr<rtsp::Request>&) noexcept
        { return false; }
    virtual bool onSetParameterRequest(std::unique_ptr<rtsp::Request>&) noexcept
        { return false; }

    virtual bool onOptionsResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
        { return rtsp::StatusCode::OK == response.statusCode; }
    virtual bool onListResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
        { return rtsp::StatusCode::OK == response.statusCode; }
    virtual bool onDescribeResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
        { return rtsp::StatusCode::OK == response.statusCode; }
    virtual bool onSetupResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
        { return rtsp::StatusCode::OK == response.statusCode; }
    virtual bool onPlayResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
        { return rtsp::StatusCode::OK == response.statusCode; }
    virtual bool onSubscribeResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
        { return rtsp::StatusCode::OK == response.statusCode; }
    virtual bool onRecordResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
        { return rtsp::StatusCode::OK == response.statusCode; }
    virtual bool onTeardownResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
        { return rtsp::StatusCode::OK == response.statusCode; }
    virtual bool onGetParameterResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
        { return rtsp::StatusCode::OK == response.statusCode; }
    virtual bool onSetParameterResponse(const rtsp::Request& request, const rtsp::Response& response) noexcept
        { return rtsp::StatusCode::OK == response.statusCode; }

private:
    QPointer<Connection> _connection;

    friend Connection;
};

}
