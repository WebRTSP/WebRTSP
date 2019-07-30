#pragma once

#include <cstddef>

#include "Token.h"


namespace rtsp {

enum class Method {
    NONE,
    OPTIONS,
    DESCRIBE,
    // ANNOUNCE,
    SETUP,
    PLAY,
    PAUSE,
    TEARDOWN,
    GET_PARAMETER,
    SET_PARAMETER,
    // REDIRECT,
    // RECORD,
};

constexpr const char* MethodName(Method) noexcept;
Method ParseMethod(const Token&) noexcept;

}
