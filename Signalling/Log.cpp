#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> WsServerLogger;


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
