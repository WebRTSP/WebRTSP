#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> RtspSessionLogger;


void InitRtspSessionLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    RtspSessionLogger = std::make_shared<spdlog::logger>("rtsp::Session", sink);

    RtspSessionLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& RtspSessionLog()
{
    if(!RtspSessionLogger)
#ifndef NDEBUG
        InitRtspSessionLogger(spdlog::level::debug);
#else
        InitRtspSessionLogger(spdlog::level::info);
#endif

    return RtspSessionLogger;
}
