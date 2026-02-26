#pragma once

#include <map>
#include <memory>

#include <QWebSocketServer>

#include "RtStreaming/GstRtStreaming/LibGst.h"

#include "../QActor.h"

#include "Session.h"


namespace webrtsp::qt {

class Server: public QWebSocketServer
{
    Q_OBJECT

public:
    Server(
        const Config* config,
        const QString& name = QString(),
        const QSslConfiguration& sslConfiguration = QSslConfiguration::defaultConfiguration()) noexcept;

signals:
    void clientAuthorized(QWebSocket*);

protected:
    virtual void clientConnected(QWebSocket*) noexcept;
    virtual void clientDisconnected(QWebSocket*) noexcept;

    virtual void textMessageReceived(QWebSocket*, const QString&) noexcept;
    virtual void binaryMessageReceived(QWebSocket*, const QByteArray&) noexcept {}

    virtual void sendTextMessage(QWebSocket*, const QString&) noexcept;
    virtual void sendBinaryMessage(QWebSocket*, const QByteArray&) noexcept;

private slots:
    void sendMessage(QWebSocket*, const std::string& message) noexcept;
    // called after session referencing connection destroy
    void connectionOrphaned(QWebSocket*) noexcept;

private:
    static void SendMessage(
        Server*,
        QWebSocket* connection,
        const std::string& message) noexcept;
    static void SendRequest(
        Server*,
        QWebSocket* connection,
        const rtsp::Request*) noexcept;
    static void SendResponse(
        Server*,
        QWebSocket* connection,
        const rtsp::Response*) noexcept;
    static void CloseConnection(
        Server*,
        QWebSocket*) noexcept;

    void closeConnection(QWebSocket* connection) noexcept;

    void handleRequest(QWebSocket*, std::unique_ptr<rtsp::Request>&&) noexcept;
    void handleResponse(QWebSocket*, std::unique_ptr<rtsp::Response>&&) noexcept;

private:
    const Config *const _config;
    const LibGst _libGst;
    Session::SharedData _sharedData;
    std::map<QWebSocket*, std::shared_ptr<Session>> _sessions;
    QActor _actor;
};

}
