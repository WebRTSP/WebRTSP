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

void Serialize(const Response& response, std::string* out) noexcept
{
    try {
        *out += ProtocolName(response.protocol);
        *out += " ";
        SerializeStatusCode(response.statusCode, out);
        *out += " ";
        *out += response.reasonPhrase;
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
            *out += "\r\n";
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
