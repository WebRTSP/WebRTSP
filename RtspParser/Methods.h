#pragma once

#include <cstddef>

#include "Token.h"


namespace rtsp {

enum class Method {
    NONE,
    OPTIONS,
    LIST,
    DESCRIBE,
    ANNOUNCE,
    SETUP,
    PLAY,
    RECORD,
//     PAUSE,
    TEARDOWN,
    GET_PARAMETER,
    SET_PARAMETER,
    // REDIRECT,
};

const char* MethodName(Method) noexcept;
Method ParseMethod(const Token&) noexcept;

}
