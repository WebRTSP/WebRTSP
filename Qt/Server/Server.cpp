#include "Server.h"

#include <QWebSocket>
#include <QTimer>
#include <QPointer>

#include "Signalling/Config.h"
#include "RtspParser/RtspParser.h"
#include "RtspParser/RtspSerialize.h"

#include "RtStreaming/GstRtStreaming/GstReStreamer2.h"


using namespace webrtsp::qt;

namespace {

enum {
    FIRST_REQUEST_WITHOUT_AUTH_DELAY = 1 // seconds
};

std::string GenerateList(const Config& config) noexcept
{
    std::string list;
    if(config.streamers.empty()) {
        list = "\r\n";
    } else {
        for(const auto& pair: config.streamers) {
            list += pair.first;
            list += ": ";
            list += pair.second.description;
            list += + "\r\n";
        }
    }

    return list;
}

Session::Streamers GenerateStreamers(const Config& config) noexcept
{
    Session::Streamers streamers;

    for(const auto& pair: config.streamers) {
        switch(pair.second.type) {
        case StreamerConfig::Type::ReStreamer:
            streamers.emplace(
                pair.first,
                std::make_unique<GstReStreamer2>(
                    pair.second.uri,
                    pair.second.forceH264ProfileLevelId));
            break;
        default:
            break;
        }
    }

    return streamers;
}

std::unique_ptr<WebRTCPeer> CreatePeer(
    Session::SharedData* sharedData,
    const std::string& uri) noexcept
{
    auto it = sharedData->streamers.find(uri);
    if(it == sharedData->streamers.end())
        return nullptr;

    return it->second->createPeer();
}

}

void Server::CloseConnection(
    Server* owner,
    QWebSocket* connection) noexcept
{
    QMetaObject::invokeMethod(
        owner,
        &Server::closeConnection,
        connection);
}

void Server::SendMessage(
    Server* owner,
    QWebSocket* connection,
    const std::string& message) noexcept
{
    QMetaObject::invokeMethod(
        owner,
        &Server::sendMessage,
        connection,
        message);
}

void Server::SendRequest(
    Server* owner,
    QWebSocket* connection,
    const rtsp::Request* request) noexcept
{
    if(!request) {
        CloseConnection(owner, connection);
        return;
    }

    const std::string serializedRequest = rtsp::Serialize(*request);
    if(serializedRequest.empty()) {
        CloseConnection(owner, connection);
        return;
    }

    SendMessage(owner, connection, serializedRequest);
}

void Server::SendResponse(
    Server* owner,
    QWebSocket* connection,
    const rtsp::Response* response) noexcept
{
    if(!response) {
        CloseConnection(owner, connection);
        return;
    }

    const std::string serializedResponse = rtsp::Serialize(*response);
    if(serializedResponse.empty()) {
        CloseConnection(owner, connection);
        return;
    }

    SendMessage(owner, connection, serializedResponse);
}


Server::Server(
    const Config* config,
    const QString& name,
    const QSslConfiguration& sslConfig) noexcept :
    QWebSocketServer(
        name,
        sslConfig.localCertificate().isNull() ?
            QWebSocketServer::NonSecureMode :
            QWebSocketServer::SecureMode,
        nullptr),
    _config(config),
    _sharedData { GenerateList(*config), GenerateStreamers(*config) }
{
    if(!sslConfig.localCertificate().isNull())
        setSslConfiguration(sslConfig);

    setSupportedSubprotocols({ "webrtsp" });
    if(listen(
        QHostAddress::Any,
        sslConfig.localCertificate().isNull() ?
            signalling::DEFAULT_WS_PORT :
            signalling::DEFAULT_WSS_PORT))
    {
        qInfo() << "WebRTSP server started on port " << serverPort();
        QObject::connect(
            this,
            &QWebSocketServer::newConnection,
            this,
            [this] () {
                if(QWebSocket* connection = nextPendingConnection()) {
                    clientConnected(connection);
                }
            });
    } else {
        qCritical() << "Faled to start WebRTSP server";
    }
}

void Server::connectionOrphaned(QWebSocket* connection) noexcept
{
    connection->deleteLater();
}

void Server::clientConnected(QWebSocket* connection) noexcept
{
    QObject::connect(
        connection,
        &QWebSocket::textMessageReceived,
        this,
        [this] (const QString& message) {
            if(QWebSocket* connection = static_cast<QWebSocket*>(sender())) {
                textMessageReceived(connection, message);
            }
        });
    QObject::connect(
        connection,
        &QWebSocket::binaryMessageReceived,
        this,
        [this] (const QByteArray& message) {
            if(QWebSocket* connection = static_cast<QWebSocket*>(sender())) {
                binaryMessageReceived(connection, message);
            }
        });
    QObject::connect(
        connection,
        &QWebSocket::disconnected,
        this,
        [this] () {
            if(QWebSocket* connection = static_cast<QWebSocket*>(sender())) {
                clientDisconnected(connection);
            }
        });

    std::shared_ptr<Session> session = std::make_shared<Session>(
        _config,
        &_sharedData,
        [sharedData = &_sharedData] (const std::string& uri) {
            return CreatePeer(sharedData, uri);
        },
        [owner = this, connection] (const rtsp::Request* request) {
            Server::SendRequest(owner, connection, request);
        },
        [owner = this, connection] (const rtsp::Response* response) {
            Server::SendResponse(owner, connection, response);
        });
    connection->setProperty("session", QVariant::fromValue(session.get()));
    _sessions.emplace(connection, session);
    session->moveToThread(_actor.actorThread());
    QPointer connectionPointer(connection);
    QObject::connect(
        session.get(), &Session::authorized,
        this, [this, connectionPointer, session] () {
            Q_ASSERT(connectionPointer);
            emit clientAuthorized(connectionPointer);
        });

    _actor.postAction([session] () {
        session->onConnected();
    });
}

void Server::clientDisconnected(QWebSocket* connection) noexcept
{
    connection->setProperty("session", QVariant());
    connection->disconnect(this);

    if(auto it = _sessions.find(connection); it != _sessions.end()) {
        std::shared_ptr<Session> session = it->second;
        _sessions.erase(it);

        _actor.postAction([owner = this, connection, session] () mutable {
            session.reset();
            // It's required to be sure main thread never receives notification
            // referencing a specific connection after connection is destroyed.
            // And it's very bad idea to keep pointer do destroyed session somewhere
            // to do validation, since there is some chance new connection will be created
            // on the same address as some previous one.
            // So since the session can be the only source of events coming from other threads
            // let's do connection destroy after session destroy for 100% guarantee of above.
            QMetaObject::invokeMethod(
                owner,
                &Server::connectionOrphaned,
                connection);
        });
    } else {
        connection->deleteLater();
    }
}

void Server::closeConnection(QWebSocket* connection) noexcept
{
    if(_sessions.find(connection) == _sessions.end())
        return;

    connection->close();
}

void Server::sendTextMessage(QWebSocket* connection, const QString& message) noexcept
{
    connection->sendTextMessage(message);
}

void Server::sendBinaryMessage(QWebSocket* connection, const QByteArray& message) noexcept
{
    connection->sendBinaryMessage(message);
}

void Server::sendMessage(QWebSocket* connection, const std::string& message) noexcept
{
    qDebug() << "WebRTSP Server ->" << message;

    sendTextMessage(connection, QString::fromStdString(message));
}

void Server::textMessageReceived(QWebSocket* connection, const QString& message) noexcept
{
    qDebug() << "WebRTSP Server <-" << message;

    const std::string tmpMessage = message.toStdString();
    if(rtsp::IsRequest(tmpMessage.data(), tmpMessage.size())) {
        std::unique_ptr<rtsp::Request> requestPtr = std::make_unique<rtsp::Request>();
        if(!rtsp::ParseRequest(tmpMessage.data(), tmpMessage.size(), requestPtr.get())) {
            qWarning()
                << "Failed to parse request:" << Qt::endl
                << message << Qt::endl
                << "Forcing disconnect...";

            closeConnection(connection);
            return;
        }

        handleRequest(connection, std::move(requestPtr));
    } else {
        std::unique_ptr<rtsp::Response> responsePtr = std::make_unique<rtsp::Response>();
        if(!rtsp::ParseResponse(tmpMessage.data(), tmpMessage.size(), responsePtr.get())) {
            qWarning()
                << "Failed to parse response:" << Qt::endl
                << message << Qt::endl
                << "Forcing disconnect...";

            closeConnection(connection);
            return;
        }

        handleResponse(connection, std::move(responsePtr));
    }
}

void Server::handleRequest(
    QWebSocket* connection,
    std::unique_ptr<rtsp::Request>&& requestPtr) noexcept
{
    Session* session = connection->property("session").value<Session*>();
    if(!session)
        return;

    if(FIRST_REQUEST_WITHOUT_AUTH_DELAY && !_config->authToken.empty()) {
        QVariant waiting = connection->property("waiting");
        if(waiting.isNull()) {
            connection->setProperty("waiting", true);

            // delay very first request to complicate token brute force
            QTimer::singleShot(
                FIRST_REQUEST_WITHOUT_AUTH_DELAY * 1000,
                connection,
                [this, connection, requestPtr = std::move(requestPtr)] () mutable {
                    connection->setProperty("waiting", false);
                    handleRequest(connection, std::move(requestPtr));
                }
            );
            return;
        } else if(waiting.toBool()) {
            // got second request without waiting very first answer
            closeConnection(connection);
            return;
        }
    }

    // it's better to use std::move_only_function here, but it's from too fresh c++23
    _actor.postAction([owner = this, connection, session, request = requestPtr.release()] () {
        if(!session->handleRequest(std::unique_ptr<rtsp::Request>(request))) {
            qWarning() << "Failed to handle request. Forcing disconnect...";
            QMetaObject::invokeMethod(
                owner,
                &Server::closeConnection,
                connection);
        }
    });
}

void Server::handleResponse(
    QWebSocket* connection,
    std::unique_ptr<rtsp::Response>&& responsePtr) noexcept
{
    Session* session = connection->property("session").value<Session*>();

    // it's better to use std::move_only_function here, but it's from too fresh c++23
    _actor.postAction([owner = this, connection, session, response = responsePtr.release()] () {
        if(!session->handleResponse(std::unique_ptr<rtsp::Response>(response))) {
            qWarning() << "Failed to handle response. Forcing disconnect...";
            QMetaObject::invokeMethod(
                owner,
                &Server::closeConnection,
                connection);
        }
    });
}
