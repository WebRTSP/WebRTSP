#include "Log.h"

#include "Helpers/SpdLog.h"


static std::shared_ptr<spdlog::logger> WsServerLogger;
static std::shared_ptr<spdlog::logger> WsClientLogger;

void InitWsServerLogger(spdlog::level::level_enum level)
{
    if(!WsServerLogger)
        WsServerLogger = CreateSpdLoggerSt("WsServer");

    WsServerLogger->set_level(level);
}

void InitWsClientLogger(spdlog::level::level_enum level)
{
    if(!WsClientLogger)
        WsClientLogger = CreateSpdLoggerSt("WsClient");

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
