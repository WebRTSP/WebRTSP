#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> Logger;

void InitInverseProxyAgentLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    Logger = std::make_shared<spdlog::logger>("InverseProxyAgent", sink);

    Logger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& InverseProxyAgentLog()
{
    if(!Logger)
#ifndef NDEBUG
        InitInverseProxyAgentLogger(spdlog::level::debug);
#else
        InitInverseProxyAgentLogger(spdlog::level::info);
#endif

    return Logger;
}
