#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> Logger;

void InitWsClientLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    Logger = std::make_shared<spdlog::logger>("WsClient", sink);

    Logger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& WsClientLog()
{
    if(!Logger)
#ifndef NDEBUG
        InitWsClientLogger(spdlog::level::debug);
#else
        InitWsClientLogger(spdlog::level::info);
#endif

    return Logger;
}
