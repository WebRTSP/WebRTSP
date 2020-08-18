#pragma once

#include <cstddef>

#include "Token.h"


namespace rtsp {

enum class Method {
    NONE,
    OPTIONS,
    LIST,
    DESCRIBE,
    // ANNOUNCE,
    SETUP,
    PLAY,
//     PAUSE,
    TEARDOWN,
    GET_PARAMETER,
    SET_PARAMETER,
    // REDIRECT,
    // RECORD,
};

const char* MethodName(Method) noexcept;
Method ParseMethod(const Token&) noexcept;

}
