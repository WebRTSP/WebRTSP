#include "Config.h"

#include <cassert>

#include <glib.h>

#include "CxxPtr/GlibPtr.h"


bool WebRTSPUrlParse(
    const char* url,
    WsClientConfig* config,
    WsClientCredentials* outCredentials,
    std::string* outPath)
{
    assert(url && config);

    if(!url)
        return false;

    g_autofree gchar* scheme = nullptr;
    gint port;
    g_autofree gchar* user = nullptr;
    g_autofree gchar* password = nullptr;
    g_autofree gchar* host = nullptr;
    g_autofree gchar* path = nullptr;
    if(g_uri_split_with_user(
        url,
        GUriFlags(G_URI_FLAGS_HAS_PASSWORD),
        &scheme,
        &user,
        &password,
        nullptr, // auth_params
        &host,
        &port,
        &path,
        nullptr, // query
        nullptr, // fragment
        nullptr))
    {
        if(!scheme || !host)
            return false;

        bool useTls;

        if(g_ascii_strcasecmp(scheme, "webrtsp") == 0) {
            useTls = false;
        } else if(g_strcmp0(scheme, "webrtsps") == 0) {
            useTls = true;
        } else {
            return false;
        }

        if(port == -1)
            port = useTls ? WEBRTSP_DEFAULT_WSS_PORT : WEBRTSP_DEFAULT_WS_PORT;

        config->server = host;
        config->serverPort = port;
        config->useTls = useTls;

        if(outCredentials) {
            if(user) {
                g_autofree char* agentId = g_uri_escape_string(user, nullptr, false);
                outCredentials->agentId = agentId;
            } else
                outCredentials->agentId.clear();

            if(password)
                outCredentials->accessToken = password;
            else
                outCredentials->accessToken.clear();
        }

        if(outPath) {
            if(path) {
                g_autofree char* escapedPath = g_uri_escape_string(
                    path[0] == '/' ? path + 1 : path,
                    "/",
                    false);
                *outPath = escapedPath;
            } else
                outPath->clear();
        }

        return true;
    }

    return false;
}
