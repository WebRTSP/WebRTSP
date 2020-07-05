#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> HttpServerLogger;


void InitHttpServerLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    HttpServerLogger = std::make_shared<spdlog::logger>("HttpServer", sink);

    HttpServerLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& HttpServerLog()
{
    if(!HttpServerLogger)
#ifndef NDEBUG
        InitHttpServerLogger(spdlog::level::debug);
#else
        InitHttpServerLogger(spdlog::level::info);
#endif

    return HttpServerLogger;
}
