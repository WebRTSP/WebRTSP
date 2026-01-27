#include "ConnectionClient.h"

#include "Connection.h"


using namespace webrtsp::qml;

ConnectionClient::ConnectionClient(Connection* connection) noexcept :
    QObject(),
    _connection(connection)
{
    _connection->registerClient(this);
}

ConnectionClient::~ConnectionClient() noexcept
{
    if(_connection)
        _connection->unregisterClient(this);
}

bool ConnectionClient::handleRequest(
    std::unique_ptr<rtsp::Request>& requestPtr) noexcept
{
    switch(requestPtr->method) {
    case rtsp::Method::NONE:
        break;
    case rtsp::Method::OPTIONS:
        return onOptionsRequest(requestPtr);
    case rtsp::Method::LIST:
        return onListRequest(requestPtr);
    case rtsp::Method::DESCRIBE:
        return onDescribeRequest(requestPtr);
    case rtsp::Method::SETUP:
        return onSetupRequest(requestPtr);
    case rtsp::Method::PLAY:
        return onPlayRequest(requestPtr);
    case rtsp::Method::SUBSCRIBE:
        return onSubscribeRequest(requestPtr);
    case rtsp::Method::RECORD:
        return onRecordRequest(requestPtr);
    case rtsp::Method::TEARDOWN:
        return onTeardownRequest(requestPtr);
    case rtsp::Method::GET_PARAMETER:
        return onGetParameterRequest(requestPtr);
    case rtsp::Method::SET_PARAMETER:
        return onSetParameterRequest(requestPtr);
    }

    return false;
}

bool ConnectionClient::handleResponse(
    const rtsp::Request& request,
    std::unique_ptr<rtsp::Response>& responsePtr) noexcept
{
    using rtsp::Method;

    switch(request.method) {
    case Method::NONE:
        break;
    case Method::OPTIONS:
        return onOptionsResponse(request, *responsePtr);
    case Method::LIST:
        return onListResponse(request, *responsePtr);
    case Method::DESCRIBE:
        return onDescribeResponse(request, *responsePtr);
    case Method::SETUP:
        return onSetupResponse(request, *responsePtr);
    case Method::PLAY:
        return onPlayResponse(request, *responsePtr);
    case Method::SUBSCRIBE:
        return onSubscribeResponse(request, *responsePtr);
    case Method::RECORD:
        return onRecordResponse(request, *responsePtr);
    case Method::TEARDOWN:
        return onTeardownResponse(request, *responsePtr);
    case Method::GET_PARAMETER:
        return onGetParameterResponse(request, *responsePtr);
    case Method::SET_PARAMETER:
        return onSetParameterResponse(request, *responsePtr);
    }

    return false;
}
