#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> Logger;

void InitLogger()
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    Logger = std::make_shared<spdlog::logger>("InverseProxyClient", sink);

#ifndef NDEBUG
    Logger->set_level(spdlog::level::debug);
#else
    Logger->set_level(spdlog::level::info);
#endif
}

const std::shared_ptr<spdlog::logger>& InverseProxyClientLog()
{
    if(!Logger)
        InitLogger();

    return Logger;
}