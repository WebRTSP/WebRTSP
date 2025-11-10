#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> WsServerLogger;
static std::shared_ptr<spdlog::logger> ServerSessionLogger;


void InitWsServerLogger(spdlog::level::level_enum level)
{
    if(!WsServerLogger) {
        WsServerLogger = spdlog::stdout_logger_st("WsServer");
#ifdef SNAPCRAFT_BUILD
        WsServerLogger->set_pattern("[%n] [%l] %v");
#endif
    }

    WsServerLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& WsServerLog()
{
    if(!WsServerLogger) {
#ifdef NDEBUG
        InitWsServerLogger(spdlog::level::info);
#else
        InitWsServerLogger(spdlog::level::debug);
#endif
    }

    return WsServerLogger;
}

void InitServerSessionLogger(spdlog::level::level_enum level)
{
    if(!ServerSessionLogger) {
        ServerSessionLogger = spdlog::stdout_logger_st("ServerSession");
#ifdef SNAPCRAFT_BUILD
        ServerSessionLogger->set_pattern("[%n] [%l] %v");
#endif
    }

    ServerSessionLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& ServerSessionLog()
{
    if(!ServerSessionLogger) {
#ifdef NDEBUG
        InitServerSessionLogger(spdlog::level::info);
#else
        InitServerSessionLogger(spdlog::level::debug);
#endif
    }

    return ServerSessionLogger;
}
