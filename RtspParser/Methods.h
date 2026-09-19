#pragma once

#include <cstddef>

#include <optional>

#include "Token.h"


namespace rtsp {

enum class Method {
    OPTIONS,
    LIST,
    DESCRIBE,
    SETUP,
    PLAY,
    SUBSCRIBE,
    RECORD,
//     PAUSE,
    TEARDOWN,
    GET_PARAMETER,
    SET_PARAMETER,
    // REDIRECT,
};

const char* MethodName(Method) noexcept;
std::optional<Method> ParseMethod(const Token&) noexcept;

}
