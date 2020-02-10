#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> WsClientLogger;
static std::shared_ptr<spdlog::logger> ClientSessionLogger;


void InitWsClientLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    WsClientLogger = std::make_shared<spdlog::logger>("WsClient", sink);

    WsClientLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& WsClientLog()
{
    if(!WsClientLogger)
#ifndef NDEBUG
        InitWsClientLogger(spdlog::level::debug);
#else
        InitWsClientLogger(spdlog::level::info);
#endif

    return WsClientLogger;
}

void InitClientSessionLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    ClientSessionLogger = std::make_shared<spdlog::logger>("ClientSession", sink);

    ClientSessionLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& ClientSessionLog()
{
    if(!ClientSessionLogger)
#ifndef NDEBUG
        InitClientSessionLogger(spdlog::level::debug);
#else
        InitClientSessionLogger(spdlog::level::info);
#endif

    return ClientSessionLogger;
}
