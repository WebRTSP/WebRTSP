#include "Connection.h"

#include <QSslConfiguration>
#include <QtWebSockets/QWebSocketHandshakeOptions>

#include "RtspParser/Response.h"
#include "RtspParser/RtspSerialize.h"
#include "RtspParser/RtspParser.h"

#include "UriInfo.h"
#include "Player.h"


using namespace webrtsp::qml;

enum {
    RECONNECT_INTERVAL_MIN = 1, // seconds
    RECONNECT_INTERVAL_MAX = 5, // seconds
    PING_INTERVAL = 60, // seconds
};

Connection::Connection(QObject* parent) noexcept :
    QObject{parent},
    rtsp::Session(
        std::make_shared<WebRTCConfig>(),
        [this] (const rtsp::Request* request) { sendRequest(request); },
        [this] (const rtsp::Response* response) { sendResponse(response); }),
    _stunServerUrl("stun://stun.cloudflare.com")
{
    updateWebRTCConfig();

    _pingTimer.setInterval(PING_INTERVAL * 1000);
    QObject::connect(&_pingTimer, &QTimer::timeout, this, &Connection::sendPing);
    _reconnectTimer.setSingleShot(true);
    QObject::connect(&_reconnectTimer, &QTimer::timeout, this, &Connection::open);
}

void Connection::registerClient(Client* client) noexcept
{
    _clients.insert(client);
}

void Connection::unregisterClient(Client* client) noexcept
{
    _clients.erase(client);

    for(auto& pair: _sentRequests) {
        if(pair.second.owner == client) {
            // have to wait all answers to handle all of them correctly
            pair.second.owner = nullptr;
        }
    }

    for(auto it = _mediaSessions.begin(); it != _mediaSessions.end();) {
        if(it->second.owner == client) {
            requestTeardown(nullptr, it->second.encodedUri, it->first);
            it = _mediaSessions.erase(it);
        } else {
            ++it;
       }
    }
}

void Connection::open() noexcept
{
    if(_webSocket) {
        qWarning() << "WebRTSP connection already opened.";
        return;
    }
    Q_ASSERT(!_webSocket);

    qDebug() << "Connecting to" << _serverUrl.toString();

    _reconnectTimer.stop();

    _webSocket = new QWebSocket(_origin, QWebSocketProtocol::VersionLatest, this);
    QObject::connect(_webSocket, &QWebSocket::connected,
        this, &Connection::socketConnected);
    QObject::connect(_webSocket, &QWebSocket::disconnected,
        this, [this] () {
            qDebug() << "Disconnected";

            QObject* webSocket = sender();

            if(_webSocket == webSocket) {
                close(true);
            }

            webSocket->deleteLater();
        });
    QObject::connect(_webSocket, &QWebSocket::textMessageReceived,
        this, &Connection::textMessageReceived);
    QObject::connect(_webSocket, &QWebSocket::binaryMessageReceived,
        this, &Connection::binaryMessageReceived);
    QObject::connect(_webSocket, &QWebSocket::textMessageReceived,
        this, &Connection::messageReceived);
    if(!_verifyCert) {
        QSslConfiguration sslConfiggration = QSslConfiguration::defaultConfiguration();
        sslConfiggration.setPeerVerifyMode(QSslSocket::VerifyNone);
        _webSocket->setSslConfiguration(sslConfiggration);
    }

    QWebSocketHandshakeOptions options;
    options.setSubprotocols({ "webrtsp" });
    _webSocket->open(_serverUrl, options);
}

void Connection::close(bool reconnect) noexcept
{
    _isOpen = false;

    rtsp::CSeq _authRequest = rtsp::InvalidCSeq;

    _pingTimer.stop();

    _reconnect = reconnect;

    for(Client* client: _clients)
        client->onDisconnected();

    _sentRequests.clear();
    _mediaSessions.clear();

    if(_webSocket) {
        _webSocket->disconnect(this);
        if(QAbstractSocket::UnconnectedState != _webSocket->state())
            _webSocket->close();
        _webSocket = nullptr;

        emit disconnected();
    }

    if(_reconnect) {
        const int delay = QRandomGenerator::global()->bounded(
                RECONNECT_INTERVAL_MIN * 1000,
                RECONNECT_INTERVAL_MAX * 1000);
        qDebug() << "Scheduled reconnect in" << delay << "ms";
        _reconnectTimer.start(delay);
    } else {
        _reconnectTimer.stop();
    }
}

void Connection::setServerUrl(const QUrl& url) noexcept
{
    if(_serverUrl == url)
        return;

    close();

    _serverUrl = url;
}

void Connection::setStunServerUrl(const QUrl& stunServerUrl) noexcept
{
    if(_stunServerUrl == stunServerUrl)
        return;

    _stunServerUrl = stunServerUrl;

    updateWebRTCConfig();
}

void Connection::setTurnServerUrl(const QUrl& turnServerUrl) noexcept
{
    if(_turnServerUrl == turnServerUrl)
        return;

    _turnServerUrl = turnServerUrl;

    updateWebRTCConfig();
}

void Connection::sendTextMessage(const QString& message) noexcept
{
    if(!_webSocket)
        return;

    _webSocket->sendTextMessage(message);
}

void Connection::sendBinaryMessage(const QByteArray& message) noexcept
{
    if(!_webSocket)
        return;

    _webSocket->sendBinaryMessage(message);
}

void Connection::sendRequest(const rtsp::Request* request) noexcept
{
    if(!_webSocket)
        return;

    if(!request) {
        close(true);
        return;
    }

    const std::string serializedRequest = rtsp::Serialize(*request);
    if(serializedRequest.empty()) {
        close(true);
        return;
    }

    qDebug() << "WebRTSPClient ->" << serializedRequest;

    sendTextMessage(QString::fromStdString(serializedRequest));
}

void Connection::sendResponse(const rtsp::Response* response) noexcept
{
    if(!_webSocket)
        return;

    if(!response) {
        close(true);
        return;
    }

    const std::string serializedResponse = rtsp::Serialize(*response);
    if(serializedResponse.empty()) {
        close(true);
        return;
    }

    qDebug() << "WebRTSPClient ->" << serializedResponse;

    sendTextMessage(QString::fromStdString(serializedResponse));
}

void Connection::socketConnected() noexcept
{
    qDebug() << "Connected";

    if(_authToken.isEmpty()) {
        authorized();
    } else {
        _authRequest = requestGetParameter(
            rtsp::WildcardUri,
            std::string(),
            std::string(),
            _authToken.toStdString());
    }
}
void Connection::updateWebRTCConfig() noexcept
{
    std::shared_ptr<WebRTCConfig> webRTCConfig = rtsp::Session::webRTCConfig() ?
        std::make_shared<WebRTCConfig>(*rtsp::Session::webRTCConfig()) :
        std::make_shared<WebRTCConfig>();

    webRTCConfig->iceServers.clear();
    webRTCConfig->iceServers.reserve(
        (_stunServerUrl.isEmpty() ? 0 : 1) + (_turnServerUrl.isEmpty() ? 0 : 1));

    if(!_stunServerUrl.isEmpty()) {
        webRTCConfig->iceServers.emplace_back(
            std::move(_stunServerUrl.toString().toStdString()));
    }

    if(!_turnServerUrl.isEmpty()) {
        webRTCConfig->iceServers.emplace_back(
            std::move(_turnServerUrl.toString().toStdString()));
    }

    setWebRTCConfig(std::move(webRTCConfig));
}

void Connection::authorized() noexcept
{
    _isOpen = true;

    _pingTimer.start();

    for(Client* client: _clients) {
        client->onConnected();
    }

    emit connected();
}

void Connection::messageReceived(const QString& message) noexcept
{
    qDebug() << "WebRTSPClient <-" << message;

    const std::string tmpMessage = message.toStdString();
    if(rtsp::IsRequest(tmpMessage.data(), tmpMessage.size())) {
        std::unique_ptr<rtsp::Request> requestPtr = std::make_unique<rtsp::Request>();
        if(!rtsp::ParseRequest(tmpMessage.data(), tmpMessage.size(), requestPtr.get())) {
            qWarning()
                << "Failed to parse request:" << Qt::endl
                << message << Qt::endl
                << "Forcing disconnect...";

            close(true);
            return;
        }

        if(!handleRequest(std::move(requestPtr))) {
            qWarning()
                << "Failed to handle request:" << Qt::endl
                << message << Qt::endl
                << "Forcing disconnect...";

            close(true);
            return;
        }
    } else {
        std::unique_ptr<rtsp::Response> responsePtr = std::make_unique<rtsp::Response>();
        if(!rtsp::ParseResponse(tmpMessage.data(), tmpMessage.size(), responsePtr.get())) {
            qWarning()
                << "Failed to parse response:" << Qt::endl
                << message << Qt::endl
                << "Forcing disconnect...";

            close(true);
            return;
        }

        if(!rtsp::Session::handleResponse(std::move(responsePtr))) {
            qWarning()
                << "Failed to handle response:" << Qt::endl
                << message << Qt::endl
                << "Forcing disconnect...";

            close(true);
            return;
        }
    }
}

bool Connection::handleRequest(
    std::unique_ptr<rtsp::Request>&& requestPtr) noexcept
{
    const rtsp::MediaSessionId mediaSession = rtsp::RequestSession(*requestPtr);

    if(mediaSession.empty()) {
        qWarning() << "Can't handle request without media session id. Forcing disconnect..." << Qt::endl;
        return false;
    }

    const auto it = _mediaSessions.find(mediaSession);
    if(it == _mediaSessions.end()) {
        // it can be request for recently destroyed player
        qWarning() << "Got request with unknown media session id" << Qt::endl;
        sendSessionNotFoundResponse(requestPtr->cseq, mediaSession);
        return true;
    }

    Client* mediaSessionOwner = it->second.owner;

    if(requestPtr->method == rtsp::Method::TEARDOWN)
        _mediaSessions.erase(mediaSession);

    return mediaSessionOwner->handleRequest(requestPtr);
}

bool Connection::handleResponse(
    const rtsp::Request& request,
    std::unique_ptr<rtsp::Response>&& responsePtr) noexcept
{
    if(_authRequest == responsePtr->cseq) {
        _authRequest = rtsp::InvalidCSeq;
        authorized();
        return true;
    }

    if(auto it = _sentRequests.find(responsePtr->cseq); it != _sentRequests.end()) {
        Client* target = it->second.owner;
        _sentRequests.erase(it);
        if(target) { // sender may not care about response
            const bool handled = target->handleResponse(request, responsePtr);
            if(handled &&
                request.method == rtsp::Method::DESCRIBE &&
                responsePtr->statusCode == rtsp::StatusCode::OK)
            {
                const rtsp::MediaSessionId mediaSession = rtsp::ResponseSession(*responsePtr);
                Q_ASSERT(!mediaSession.empty());
                if(!mediaSession.empty())
                    _mediaSessions.emplace(mediaSession, MediaSessionData { request.uri, target });
            }
            return handled;
        } else if(request.method == rtsp::Method::DESCRIBE &&
            responsePtr->statusCode == rtsp::StatusCode::OK)
        {
            // it's highly possible target was destroyed before receive answer for DESCRIBE.
            // have to force media session TEARDOWN
            const rtsp::MediaSessionId mediaSession = rtsp::ResponseSession(*responsePtr);
            if(!mediaSession.empty())
                requestTeardown(nullptr, request.uri, mediaSession);

            return true;
        } else {
            return true;
        }
    }

    const bool isPing =
        request.method == rtsp::Method::GET_PARAMETER &&
        request.headerFields.empty() &&
        request.body.empty();

    return isPing; // ping answer is never hendled
}

rtsp::CSeq Connection::requestOptions(Client* source, const std::string& encodedUri) noexcept
{
    if(!_isOpen) {
        Q_ASSERT(false);
        return rtsp::InvalidCSeq;
    }

    const rtsp::CSeq cseq = rtsp::Session::requestOptions(encodedUri);

    _sentRequests.emplace(cseq, RequestData { source });

    return cseq;
}

rtsp::CSeq Connection::requestList(Client* source, const std::string& encodedUri) noexcept
{
    if(!_isOpen) {
        Q_ASSERT(false);
        return rtsp::InvalidCSeq;
    }

    const rtsp::CSeq cseq = rtsp::Session::requestList(encodedUri);

    _sentRequests.emplace(cseq, RequestData { source });

    return cseq;
}

rtsp::CSeq Connection::requestDescribe(Client* source, const std::string& encodedUri) noexcept
{
    if(!_isOpen) {
        Q_ASSERT(false);
        return rtsp::InvalidCSeq;
    }

    const rtsp::CSeq cseq = rtsp::Session::requestDescribe(encodedUri);

    _sentRequests.emplace(cseq, RequestData { source });

    return cseq;
}

rtsp::CSeq Connection::requestPlay(
    Client* source,
    const std::string& encodedUri,
    const rtsp::MediaSessionId& mediaSession,
    const std::string& sdp) noexcept
{
    if(!_isOpen) {
        Q_ASSERT(false);
        return rtsp::InvalidCSeq;
    }

    const rtsp::CSeq cseq = rtsp::Session::requestPlay(
        encodedUri,
        mediaSession,
        sdp);

    _sentRequests.emplace(cseq, RequestData { source });

    return cseq;
}

rtsp::CSeq Connection::requestSetup(
    Client* source,
    const std::string& encodedUri,
    const rtsp::MediaSessionId& mediaSession,
    unsigned mlineIndex,
    const std::string& candidate) noexcept
{
    if(!_isOpen) {
        Q_ASSERT(false);
        return rtsp::InvalidCSeq;
    }

    const rtsp::CSeq cseq = rtsp::Session::requestSetup(
        encodedUri,
        rtsp::IceCandidateContentType,
        mediaSession,
        std::to_string(mlineIndex) + "/" + candidate + "\r\n");

    _sentRequests.emplace(cseq, RequestData { source });

    return cseq;
}

rtsp::CSeq Connection::requestTeardown(
    Client* source,
    const std::string& encodedUri,
    const rtsp::MediaSessionId& mediaSession) noexcept
{
    if(!_isOpen) {
        Q_ASSERT(false);
        return rtsp::InvalidCSeq;
    }

    const rtsp::CSeq cseq = rtsp::Session::requestTeardown(encodedUri, mediaSession);

    _sentRequests.emplace(cseq, RequestData { source });

    return cseq;
}

void Connection::sendPing() noexcept
{
    if(!_isOpen) {
        Q_ASSERT(false);
        return;
    }

    rtsp::Session::requestGetParameter("*", std::string(), std::string());
}

UriInfo* Connection::uriInfo(const QString& uri)
{
    return new UriInfo(this, uri);
}

Player* Connection::player(const QString& uri, QQuickItem* view)
{
    return new Player(this, uri, view);
}
