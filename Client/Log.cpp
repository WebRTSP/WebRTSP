#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> WsClientLogger;
static std::shared_ptr<spdlog::logger> ClientSessionLogger;


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

void InitClientSessionLogger(spdlog::level::level_enum level)
{
    if(!ClientSessionLogger) {
        ClientSessionLogger = spdlog::stdout_logger_st("ClientSession");
#ifdef SNAPCRAFT_BUILD
        ClientSessionLogger->set_pattern("[%n] [%l] %v");
#endif
    }

    ClientSessionLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& ClientSessionLog()
{
    if(!ClientSessionLogger) {
#ifdef NDEBUG
        InitClientSessionLogger(spdlog::level::info);
#else
        InitClientSessionLogger(spdlog::level::debug);
#endif
    }

    return ClientSessionLogger;
}

std::shared_ptr<spdlog::logger> MakeClientSessionLogger(const std::string& context)
{
    const std::shared_ptr<spdlog::logger>& logger = ClientSessionLog();

    if(context.empty()) {
        return logger;
    } else {
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
}
