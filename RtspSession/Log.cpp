#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> RtspSessionLogger;
static std::shared_ptr<spdlog::logger> ClientSessionLogger;
static std::shared_ptr<spdlog::logger> ServerSessionLogger;


void InitRtspSessionLogger(spdlog::level::level_enum level)
{
    if(!RtspSessionLogger) {
        RtspSessionLogger = spdlog::stdout_logger_st("rtsp::Session");
#ifdef SNAPCRAFT_BUILD
        RtspSessionLogger->set_pattern("[%n] [%l] %v");
#endif
    }

    RtspSessionLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& RtspSessionLog()
{
    if(!RtspSessionLogger) {
#ifdef NDEBUG
        InitRtspSessionLogger(spdlog::level::info);
#else
        InitRtspSessionLogger(spdlog::level::debug);
#endif
    }

    return RtspSessionLogger;
}

std::shared_ptr<spdlog::logger> MakeRtspSessionLogger(const std::string& context)
{
    const std::shared_ptr<spdlog::logger>& logger = RtspSessionLog();

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
