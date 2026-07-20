#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> WsServerLogger;
static std::shared_ptr<spdlog::logger> WsClientLogger;

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

void InitWsClientLogger(spdlog::level::level_enum level)
{
    if(!WsClientLogger) {
        WsClientLogger = spdlog::stdout_logger_st("WsClient");
#ifdef SNAPCRAFT_BUILD
        WsClientLogger->set_pattern("[%n] [%l] %v");
#endif
    }

    WsClientLogger->set_level(level);
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

const std::shared_ptr<spdlog::logger>& WsClientLog()
{
    if(!WsClientLogger) {
#ifdef NDEBUG
        InitWsClientLogger(spdlog::level::info);
#else
        InitWsClientLogger(spdlog::level::debug);
#endif
    }

    return WsClientLogger;
}
