#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> WsServerLogger;
static std::shared_ptr<spdlog::logger> ServerSessionLogger;


void InitWsServerLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    WsServerLogger = std::make_shared<spdlog::logger>("WsServer", sink);

    WsServerLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& WsServerLog()
{
    if(!WsServerLogger)
#ifndef NDEBUG
        InitWsServerLogger(spdlog::level::debug);
#else
        InitWsServerLogger(spdlog::level::info);
#endif

    return WsServerLogger;
}

void InitServerSessionLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    ServerSessionLogger = std::make_shared<spdlog::logger>("ServerSession", sink);

    ServerSessionLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& ServerSessionLog()
{
    if(!ServerSessionLogger)
#ifndef NDEBUG
        InitServerSessionLogger(spdlog::level::debug);
#else
        InitServerSessionLogger(spdlog::level::info);
#endif

    return ServerSessionLogger;
}
