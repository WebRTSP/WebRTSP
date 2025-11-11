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

std::shared_ptr<spdlog::logger> MakeServerSessionLogger(const std::string& context)
{
    const std::shared_ptr<spdlog::logger>& logger = ServerSessionLog();

    if(!context.empty()) {
        // have to go long road to avoid issues with duplicated names in loggers registry
        std::shared_ptr<spdlog::logger> loggerWithContext = std::make_shared<spdlog::logger>(
            logger->name(),
            std::make_shared<spdlog::sinks::stdout_sink_st>());
        loggerWithContext->set_level(logger->level());

#ifdef SNAPCRAFT_BUILD
        loggerWithContext->set_pattern("[" + context + "] [%n] [%l] %v");
#else
        loggerWithContext->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [" + context + "] [%n] [%l] %v");
#endif

        return loggerWithContext;
    }

    return logger;
}
