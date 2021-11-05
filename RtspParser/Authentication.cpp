#include "Authentication.h"

#include <cstring>


namespace rtsp {

namespace {

static const Authentication Authentications[] = {
    Authentication::Bearer,
};

static const unsigned AuthenticationsCount =
    sizeof(Authentications) / sizeof(Authentications[0]);

}

const char* AuthenticationName(Authentication authentication) noexcept
{
    switch(authentication) {
    case Authentication::None:
        return nullptr;
    case Authentication::Unknown:
        return nullptr;
    case Authentication::Bearer:
        return "Bearer";
    }

    return nullptr;
}

Authentication ParseAuthentication(const Token& token) noexcept
{
    if(IsEmptyToken(token))
        return Authentication::Unknown;

    for(const Authentication a: Authentications) {
        const char* authName = AuthenticationName(a);
        if(0 == strncmp(authName, token.token, token.size) && strlen(authName) == token.size)
            return a;
    }

    return Authentication::Unknown;
}

}
