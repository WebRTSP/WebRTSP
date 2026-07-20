#include "Config.h"

#include <cassert>

#include <glib.h>

#include "CxxPtr/GlibPtr.h"
#include "Signalling/Config.h"


namespace client {

bool FillConfigFromUrl(const char* url, Config* config)
{
    assert(config);

    gchar* scheme;
    gchar* host;
    gint port;
    if(g_uri_split_network(
        url,
        G_URI_FLAGS_NONE,
        &scheme,
        &host,
        &port,
        nullptr))
    {
        GCharPtr schemePtr(scheme);
        GCharPtr hostPtr(host);

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

        if(port == -1) {
            using namespace signalling;
            port = useTls ? DEFAULT_WSS_PORT : DEFAULT_WS_PORT;
        }

        config->server = host;
        config->serverPort = port;
        config->useTls = useTls;

        return true;
    }

    return false;
}

}
