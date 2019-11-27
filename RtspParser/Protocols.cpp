#include "Protocols.h"

#include <cstring>


namespace rtsp {

namespace {

static const Protocol Protocols[] = {
    Protocol::WEBRTSP_0_1,
};

static const unsigned ProtocolsCount = sizeof(Protocols) / sizeof(Protocols[0]);

}

const char* ProtocolName(Protocol protocol) noexcept
{
    switch(protocol) {
    case Protocol::NONE:
        return nullptr;
    case Protocol::WEBRTSP_0_1:
        return "WEBRTSP/0.1";
    }

    return nullptr;
}

Protocol ParseProtocol(const Token& token) noexcept
{
    if(IsEmptyToken(token))
        return Protocol::NONE;

    for(const Protocol p: Protocols) {
        const char* protocolName = ProtocolName(p);
        if(0 == strncmp(protocolName, token.token, token.size) && strlen(protocolName) == token.size)
            return p;
    }

    return Protocol::NONE;
}

}
