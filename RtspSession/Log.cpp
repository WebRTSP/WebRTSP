#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> RtspSessionLogger;


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
