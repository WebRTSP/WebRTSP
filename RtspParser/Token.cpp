#include "Token.h"

#include <cassert>


namespace rtsp {

bool IsEmptyToken(const Token& token) noexcept
{
    assert(
        (token.token == nullptr && token.size == 0) ||
        (token.token != nullptr && token.size > 0));

    return token.token == nullptr || token.size == 0;
}

}
