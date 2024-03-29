#include "Methods.h"

#include <cstring>

#include "Token.h"


namespace rtsp {

namespace {

static const Method Methods[] = {
    Method::OPTIONS,
    Method::LIST,
    Method::DESCRIBE,
    Method::SETUP,
    Method::PLAY,
    Method::RECORD,
    Method::SUBSCRIBE,
    //Method::PAUSE,
    Method::TEARDOWN,
    Method::GET_PARAMETER,
    Method::SET_PARAMETER,
    //Method::REDIRECT,
};

static const unsigned MethodsCount = sizeof(Methods) / sizeof(Methods[0]);

}

const char* MethodName(Method method) noexcept
{
    switch(method) {
    case Method::NONE:
        return nullptr;
    case Method::OPTIONS:
        return "OPTIONS";
    case Method::LIST:
        return "LIST";
    case Method::DESCRIBE:
        return "DESCRIBE";
    case Method::SETUP:
        return "SETUP";
    case Method::PLAY:
        return "PLAY";
    case Method::RECORD:
        return "RECORD";
    case Method::SUBSCRIBE:
        return "SUBSCRIBE";
    // case Method::PAUSE:
    //     return "PAUSE";
    case Method::TEARDOWN:
        return "TEARDOWN";
    case Method::GET_PARAMETER:
        return "GET_PARAMETER";
    case Method::SET_PARAMETER:
        return "SET_PARAMETER";
    // case Method::REDIRECT:
    //     return "REDIRECT";
    }

    return nullptr;
}

Method ParseMethod(const Token& token) noexcept
{
    if(IsEmptyToken(token))
        return Method::NONE;

    for(const Method m: Methods) {
        const char* methodName = MethodName(m);
        if(0 == strncmp(methodName, token.token, token.size) && strlen(methodName) == token.size)
            return m;
    }

    return Method::NONE;
}

}
