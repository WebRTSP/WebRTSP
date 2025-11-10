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
