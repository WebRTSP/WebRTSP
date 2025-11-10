#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> HttpServerLogger;


void InitHttpServerLogger(spdlog::level::level_enum level)
{
    if(!HttpServerLogger) {
        HttpServerLogger = spdlog::stdout_logger_st("HttpServer");
#ifdef SNAPCRAFT_BUILD
        HttpServerLogger->set_pattern("[%n] [%l] %v");
#endif
    }

    HttpServerLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& HttpServerLog()
{
    if(!HttpServerLogger) {
#ifdef NDEBUG
        InitHttpServerLogger(spdlog::level::info);
#else
        InitHttpServerLogger(spdlog::level::debug);
#endif
    }

    return HttpServerLogger;
}
