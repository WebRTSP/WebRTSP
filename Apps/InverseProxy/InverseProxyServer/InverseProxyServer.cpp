#include "InverseProxyServer.h"

#include <memory>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/libwebsocketsPtr.h>

#include "Signalling/WsServer.h"

#include "FrontSession.h"
#include "BackSession.h"


int InverseProxyServerMain(const InverseProxyServerConfig& config)
{
    GMainLoopPtr loopPtr(g_main_loop_new(nullptr, FALSE));
    GMainLoop* loop = loopPtr.get();

    lws_context_creation_info wsInfo {};
    wsInfo.gid = -1;
    wsInfo.uid = -1;
    wsInfo.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;

    LwsContextPtr contextPtr(lws_create_context(&wsInfo));
    lws_context* context = contextPtr.get();

    Forwarder forwarder(config);

    signalling::Config frontConfig {
        .serverName = config.serverName,
        .certificate = config.certificate,
        .key = config.key,
        .bindToLoopbackOnly = true,
        .port = config.frontPort,
        .secureBindToLoopbackOnly = false,
        .securePort = config.secureFrontPort,
    };
    signalling::WsServer frontServer(
        frontConfig, loop,
        std::bind(
            &Forwarder::createFrontSession, &forwarder,
            std::placeholders::_1, std::placeholders::_2));

    signalling::Config backConfig {
        .serverName = config.serverName,
        .certificate = config.certificate,
        .key = config.key,
        .bindToLoopbackOnly = true,
        .port = config.backPort,
        .secureBindToLoopbackOnly = false,
        .securePort = config.secureBackPort,
    };
    signalling::WsServer backServer(
        backConfig, loop,
        std::bind(
            &Forwarder::createBackSession, &forwarder,
            std::placeholders::_1, std::placeholders::_2));

    if(frontServer.init(context) && backServer.init(context))
        g_main_loop_run(loop);
    else
        return -1;

    return 0;
}
