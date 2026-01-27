#pragma once

#include <QObject>
#include <QtQml>

#include "ConnectionClient.h"


namespace webrtsp::qml {

struct Uri2Info
{
    Q_GADGET
    QML_VALUE_TYPE(uri2info)
    QML_UNCREATABLE("Type is only available as property value")

public:
    Q_PROPERTY(QString uri READ uri)
    Q_PROPERTY(QString info READ info)

    Uri2Info() {}
    Uri2Info(const QString& uri, const QString& info);
    Uri2Info(const Uri2Info&);

    Uri2Info& operator = (const Uri2Info&);

    QString uri() const noexcept { return _uri; }
    QString info() const noexcept { return _info; }

private:
    QString _uri;
    QString _info;
};

class UriInfo: public ConnectionClient
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WebRTSPUriInfo)
    QML_UNCREATABLE("Type is only available as property value")

public:
    typedef rtsp::Method Options;
    Q_ENUM(Options)
    Q_PROPERTY(QString uri READ uri)
    Q_PROPERTY(const QList<Options>& options READ options NOTIFY optionsChanged)
    Q_PROPERTY(const QList<Uri2Info>& list READ list NOTIFY listChanged)

    UriInfo(Connection* connection, const QString& uri) noexcept;

    QString uri() const noexcept { return _uri; }
    const QList<Options>& options() noexcept;
    const QList<Uri2Info>& list() noexcept;

signals:
    void optionsChanged(const QList<rtsp::Method>&);
    void listChanged(const QList<Uri2Info>&);

private:
    void onConnected() noexcept override;
    void onDisconnected() noexcept override;

    bool onOptionsResponse(
        const rtsp::Request& request,
        const rtsp::Response& response) noexcept override;
    bool onListResponse(
        const rtsp::Request& request,
        const rtsp::Response& response) noexcept override;

private:
    const QString _uri;
    const std::string _encodedUri;

    rtsp::CSeq _optionsCSeq = rtsp::InvalidCSeq;
    rtsp::CSeq _listCSeq = rtsp::InvalidCSeq;
    QList<Options> _options;
    QList<Uri2Info> _list;
};

}
