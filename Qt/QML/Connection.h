#pragma once

#include <QObject>
#include <QtQml>
#include <QQuickItem>
#include <QWebSocket>

#include "RtspSession/Session.h"

#include "UriInfo.h"
#include "Player.h"


namespace webrtsp::qml {

class ConnectionClient;
class Connection: public QObject, private rtsp::Session
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WebRTSPConnection)

public:
    typedef ConnectionClient Client;

 private:
    struct RequestData {
        Client* owner;
    };

    struct MediaSessionData {
        std::string encodedUri;
        Client* owner;
    };

public:
    Q_PROPERTY(QUrl serverUrl MEMBER _serverUrl WRITE setServerUrl)
    Q_PROPERTY(QUrl stunServerUrl MEMBER _stunServerUrl WRITE setStunServerUrl)
    Q_PROPERTY(QUrl turnServerUrl MEMBER _turnServerUrl WRITE setTurnServerUrl)
    Q_PROPERTY(QString origin MEMBER _origin WRITE setOrigin)
    Q_PROPERTY(bool verifyCert MEMBER _verifyCert WRITE setVerifyCert)
    Q_PROPERTY(QString authToken MEMBER _authToken WRITE setAuthToken)
    Q_PROPERTY(bool isOpen READ isOpen)

    explicit Connection(QObject* parent = nullptr) noexcept;

    void setServerUrl(const QUrl&) noexcept;
    void setStunServerUrl(const QUrl&) noexcept;
    void setTurnServerUrl(const QUrl&) noexcept;

    using rtsp::Session::webRTCConfig;

    void setOrigin(const QString& origin) noexcept { _origin = origin; }
    void setVerifyCert(bool verifyCert) noexcept { _verifyCert = verifyCert; }
    void setAuthToken(const QString& authToken) noexcept { _authToken = authToken; }

    Q_INVOKABLE bool isOpen() const noexcept { return _isOpen; }

    Q_INVOKABLE void open() noexcept;
    Q_INVOKABLE void close() noexcept { close(false); }

    Q_INVOKABLE UriInfo* uriInfo(const QString& uri);
    Q_INVOKABLE Player* player(const QString& uri, QQuickItem* view);

    rtsp::CSeq requestOptions(Client* source, const std::string& encodedUri) noexcept;
    rtsp::CSeq requestList(Client* source, const std::string& encodedUri) noexcept;
    rtsp::CSeq requestDescribe(Client* source, const std::string& encodedUri) noexcept;
    rtsp::CSeq requestPlay(
        Client* source,
        const std::string& encodedUri,
        const rtsp::MediaSessionId&,
        const std::string& sdp) noexcept;
    rtsp::CSeq requestSetup(
        Client* source,
        const std::string& encodedUri,
        const rtsp::MediaSessionId& mediaSession,
        unsigned mlineIndex,
        const std::string& candidate) noexcept;
    rtsp::CSeq requestTeardown(
        Client* source,
        const std::string& encodedUri,
        const rtsp::MediaSessionId&) noexcept;

    using rtsp::Session::sendOkResponse;

signals:
    void connected();
    void disconnected();

    void textMessageReceived(const QString&);
    void binaryMessageReceived(const QByteArray&);

public slots:
    virtual void sendTextMessage(const QString&) noexcept;
    virtual void sendBinaryMessage(const QByteArray&) noexcept;

protected slots:
    virtual void messageReceived(const QString&) noexcept;

private:
    void updateWebRTCConfig() noexcept;

    void authorized() noexcept;

    void registerClient(Client*) noexcept;
    void unregisterClient(Client*) noexcept;

    void sendRequest(const rtsp::Request*) noexcept;
    void sendResponse(const rtsp::Response*) noexcept;

    bool handleRequest(
        std::unique_ptr<rtsp::Request>&&) noexcept override;
    bool handleResponse(
        const rtsp::Request&,
        std::unique_ptr<rtsp::Response>&&) noexcept override;

    void sendPing() noexcept;

    void close(bool reconnect) noexcept;

private slots:
    void socketConnected() noexcept;

private:
    QUrl _serverUrl;
    QUrl _stunServerUrl;
    QUrl _turnServerUrl;

    QString _origin;
    QString _authToken;
    bool _verifyCert = true;
    bool _reconnect = false;
    QWebSocket* _webSocket = nullptr;
    bool _isOpen = false;

    rtsp::CSeq _authRequest = rtsp::InvalidCSeq;

    std::set<Client*> _clients;
    std::map<rtsp::CSeq, RequestData> _sentRequests;
    std::map<rtsp::MediaSessionId, MediaSessionData> _mediaSessions;

    QTimer _pingTimer;
    QTimer _reconnectTimer;

    friend ConnectionClient;
};

}
