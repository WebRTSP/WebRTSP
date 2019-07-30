#pragma once

#include <cstddef>


namespace rtsp {

struct Token
{
    const char* token = nullptr;
    size_t size = 0;
};

bool IsEmptyToken(const Token& token) noexcept;

}

