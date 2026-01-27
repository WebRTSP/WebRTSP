#include "UriInfo.h"

#include "RtspParser/RtspParser.h"

#include "Connection.h"


using namespace webrtsp::qml;

Uri2Info::Uri2Info(const QString& uri, const QString& info) :
    _uri(uri), _info(info)
{
}

Uri2Info::Uri2Info(const Uri2Info& source) :
    _uri(source._uri), _info(source._info)
{
}

Uri2Info& Uri2Info::operator = (const Uri2Info& source)
{
    _uri = source._uri;
    _info = source._info;

    return *this;
}


UriInfo::UriInfo(Connection* connection, const QString& uri) noexcept :
    ConnectionClient::ConnectionClient(connection),
    _uri(uri),
    _encodedUri(uri == "*" ? uri.toStdString() : QUrl::toPercentEncoding(uri).toStdString())
{
}

void UriInfo::onConnected() noexcept
{
    _optionsCSeq = connection()->requestOptions(this, _encodedUri);
}

void UriInfo::onDisconnected() noexcept
{
    _optionsCSeq = rtsp::InvalidCSeq;
    _listCSeq = rtsp::InvalidCSeq;

    const bool optionsChanged = !_options.empty();
    const bool listChanged = !_list.empty();

    if(!_options.empty()) {
        _options.clear();
    }

    if(!_list.empty()) {
        _list.clear();
    }

    if(optionsChanged) {
        emit this->optionsChanged(options());
    }

    if(listChanged) {
        emit this->listChanged(list());
    }
}

const QList<UriInfo::Options>& UriInfo::options() noexcept
{
    return _options;
}

const QList<Uri2Info>& UriInfo::list() noexcept
{
    return _list;
}

bool UriInfo::onOptionsResponse(
    const rtsp::Request&,
    const rtsp::Response& response) noexcept
{
    if(_optionsCSeq != response.cseq)
        return true;

    _optionsCSeq = rtsp::InvalidCSeq;

    _options.clear();
    const std::set<rtsp::Method> options = ParseOptions(response);
    for(const rtsp::Method option: options) {
        _options.append(option);
    }
    emit optionsChanged(this->options());

    if(options.count(rtsp::Method::LIST)) {
        _listCSeq = connection()->requestList(this, _encodedUri);
    } else {
        _list.clear();
        emit listChanged(list());
    }

    return true;
}

bool UriInfo::onListResponse(
    const rtsp::Request&,
    const rtsp::Response& response) noexcept
{
    if(_listCSeq != response.cseq)
        return true;

    _listCSeq = rtsp::InvalidCSeq;

    rtsp::Parameters parameters;
    if(!rtsp::ParseParameters(response.body, &parameters))
        return false;

    _list.clear();
    for(const auto& pair: parameters) {
        _list.emplace_back(
            QUrl::fromPercentEncoding(QByteArray::fromStdString(pair.first)),
            QUrl::fromPercentEncoding(QByteArray::fromStdString(pair.second)));
    }

    emit listChanged(list());

    return true;
}
