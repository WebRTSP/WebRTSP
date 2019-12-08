#include "RtspSerialize.h"


namespace rtsp {

namespace {

void SerializeStatusCode(unsigned statusCode, std::string* out)
{
    out->reserve(out->size() + 3);

    if(statusCode > 999) {
        *out += "999";
    } else if(statusCode < 100) {
        *out += "100";
    } else {
        out->resize(out->size() + 3);
        for(unsigned i = 0; i < 3; ++i) {
            (*out)[out->size() - i - 1] = '0' + statusCode % 10;
            statusCode /= 10;
        }
    }
}

}

void Serialize(const Request& request, std::string* out) noexcept
{
    try {
        *out = MethodName(request.method);
        *out += " ";
        *out += request.uri;
        *out += " ";
        *out += ProtocolName(request.protocol);
        *out += "\r\n";

        *out += "CSeq: ";
        *out += std::to_string(request.cseq);
        *out += "\r\n";

        for(const std::pair<std::string, std::string>& hf: request.headerFields) {
            *out += hf.first;
            *out += ": ";
            *out += hf.second;
            *out += "\r\n";
        }

        if(!request.body.empty()) {
            *out +="\r\n";
            *out += request.body;
        }
    } catch(...) {
        out->clear();
    }
}

std::string Serialize(const Request& request) noexcept
{
    std::string out;
    Serialize(request, &out);
    return out;
}

void Serialize(const Response& response, std::string* out) noexcept
{
    try {
        *out = ProtocolName(response.protocol);
        *out += " ";
        SerializeStatusCode(response.statusCode, out);
        *out += " ";
        *out += response.reasonPhrase;
        *out += "\r\n";

        *out += "CSeq: ";
        *out += std::to_string(response.cseq);
        *out += "\r\n";

        for(const std::pair<std::string, std::string>& hf: response.headerFields) {
            *out += hf.first;
            *out += ": ";
            *out += hf.second;
            *out += "\r\n";
        }

        if(!response.body.empty()) {
            *out +="\r\n";
            *out += response.body;
        }
    } catch(...) {
        out->clear();
    }
}

std::string Serialize(const Response& response) noexcept
{
    std::string out;
    Serialize(response, &out);
    return out;
}

}
