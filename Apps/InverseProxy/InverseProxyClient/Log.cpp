#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> Logger;

void InitInverseProxyClientLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    Logger = std::make_shared<spdlog::logger>("InverseProxyClient", sink);

    Logger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& InverseProxyClientLog()
{
    if(!Logger)
#ifndef NDEBUG
        InitInverseProxyClientLogger(spdlog::level::debug);
#else
        InitInverseProxyClientLogger(spdlog::level::info);
#endif

    return Logger;
}
