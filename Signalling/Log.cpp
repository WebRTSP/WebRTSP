#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> Logger;

void InitWsServerLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    Logger = std::make_shared<spdlog::logger>("WsServer", sink);

    Logger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& WsServerLog()
{
    if(!Logger)
#ifndef NDEBUG
        InitWsServerLogger(spdlog::level::debug);
#else
        InitWsServerLogger(spdlog::level::info);
#endif

    return Logger;
}
