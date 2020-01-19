#include "TurnRestApi.h"

#include <glib.h>

#include <cassert>
#include <vector>

std::string TurnTemporaryUsername(
    const std::string& temporaryUsername,
    unsigned passwordTTL)
{
    const time_t now = time(nullptr);

    if(temporaryUsername.empty())
        return std::to_string(now + passwordTTL) + ":" + temporaryUsername;
    else
        return std::to_string(now + passwordTTL) + ":" + temporaryUsername;
}

std::string TurnTemporaryPassword(
    const std::string& temporaryUsername,
    const std::string& staticAuthSecret)
{
    const GChecksumType type = G_CHECKSUM_SHA1;
    const gssize digestSize = g_checksum_type_get_length(type);
    if(-1 == digestSize)
        return std::string();

    GHmac* hmac =
        g_hmac_new(
            type,
            reinterpret_cast<const guchar*>(staticAuthSecret.c_str()),
            staticAuthSecret.size());
    if(hmac) {
        g_hmac_update(
            hmac,
            reinterpret_cast<const guchar*>(temporaryUsername.c_str()),
            temporaryUsername.size());

        std::vector<guint8> digest(digestSize);
        gsize digestLen;
        g_hmac_get_digest(hmac, digest.data(), &digestLen);
        assert(digestLen == static_cast<gsize>(digestSize));

        g_hmac_unref(hmac);

        gchar* base64 = g_base64_encode(digest.data(), digest.size());
        const std::string result(base64);
        g_free(base64);

        return result;
    }

    return std::string();
}

std::string IceServer(
    const std::string& username,
    unsigned passwordTTL,
    const std::string& staticAuthSecret,
    const std::string& iceEndpoint)
{
    const std::string userName =
        TurnTemporaryUsername(username, passwordTTL);

    const std::string password =
        TurnTemporaryPassword(userName, staticAuthSecret);

    char* escapedUserName = g_uri_escape_string(userName.c_str(), nullptr, false);
    std::string escapedUserName2 = escapedUserName;
    g_free(escapedUserName);

    char* escapedPassword = g_uri_escape_string(password.c_str(), nullptr, false);
    std::string escapedPassword2 = escapedPassword;
    g_free(escapedPassword);

    return escapedUserName2 + ":" + escapedPassword2 + "@" + iceEndpoint;
}
