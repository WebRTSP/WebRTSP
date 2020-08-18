#include "Methods.h"

#include <cstring>

#include "Token.h"


namespace rtsp {

namespace {

static const Method Methods[] = {
    Method::OPTIONS,
    Method::LIST,
    Method::DESCRIBE,
    //Method::ANNOUNCE,
    Method::SETUP,
    Method::PLAY,
    //Method::PAUSE,
    Method::TEARDOWN,
    Method::GET_PARAMETER,
    Method::SET_PARAMETER,
    //Method::REDIRECT,
    //Method::RECORD,
};

static const unsigned MethodsCount = sizeof(Methods) / sizeof(Methods[0]);

}

const char* MethodName(Method method) noexcept
{
    switch(method) {
    case Method::NONE:
        return nullptr;
    case Method::LIST:
        return "LIST";
    case Method::OPTIONS:
        return "OPTIONS";
    case Method::DESCRIBE:
        return "DESCRIBE";
    // case Method::ANNOUNCE:
    //     return "ANNOUNCE";
    case Method::SETUP:
        return "SETUP";
    case Method::PLAY:
        return "PLAY";
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
    // case Method::RECORD:
    //     return "RECORD";
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
