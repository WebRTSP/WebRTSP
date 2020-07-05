#include "LwsLog.h"

#include <libwebsockets.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


static std::shared_ptr<spdlog::logger> Logger;


static int PrepareLwsLogLevel(int maxLevel);
static void LwsLog(int level, const char* line);

void InitLwsLogger(spdlog::level::level_enum level)
{
    spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_st>();

    Logger = std::make_shared<spdlog::logger>("libwebsockets", sink);

    Logger->set_level(level);

    int lwsLogLevel = LLL_ERR | LLL_WARN | LLL_NOTICE;
    switch(level) {
        case spdlog::level::trace:
            lwsLogLevel = PrepareLwsLogLevel(LLL_THREAD);
            break;
        case spdlog::level::debug:
            lwsLogLevel = PrepareLwsLogLevel(LLL_DEBUG);
            break;
        case spdlog::level::info:
            lwsLogLevel = PrepareLwsLogLevel(LLL_INFO);
            break;
        case spdlog::level::warn:
            lwsLogLevel = PrepareLwsLogLevel(LLL_WARN);
            break;
        case spdlog::level::err:
        case spdlog::level::critical:
            lwsLogLevel = PrepareLwsLogLevel(LLL_ERR);
            break;
        case spdlog::level::off:
            lwsLogLevel = 0;
            break;
    }

    lws_set_log_level(lwsLogLevel, LwsLog);
}

static int PrepareLwsLogLevel(int maxLevel)
{
    int level = maxLevel;
    int outLevel = level;

    while(level > 1) {
        level >>= 1;
        outLevel |= level;
    }

    return outLevel;
}

static void LwsLog(int level, const char* line)
{
    if(nullptr == line)
        return;

    std::string logLines;
    size_t blockPos = 0;
    size_t pos = 0;
    for(; line[pos] != 0; ++pos) {
        if('\r' == line[pos]) {
            logLines.reserve(logLines.size() + pos - blockPos);
            logLines.append(line + blockPos, pos - blockPos);
            blockPos = pos + 1;
        }
    }

    if(blockPos < pos - 1) {
        logLines.reserve(logLines.size() + pos - blockPos);
        logLines.append(line + blockPos, pos - blockPos);
    }

    const std::string::size_type lastChar = logLines.find_last_not_of('\n');
    if(std::string::npos == lastChar)
        return;

    logLines.resize(lastChar + 1);

    if(logLines.empty())
        return;

    switch(level) {
        case LLL_ERR:
            Logger->error(logLines);
            break;
        case LLL_WARN:
            Logger->warn(logLines);
            break;
        case LLL_NOTICE:
        case LLL_INFO:
            Logger->info(logLines);
            break;
        case LLL_DEBUG:
            Logger->debug(logLines);
            break;
        case LLL_PARSER:
        case LLL_HEADER:
        case LLL_EXT:
        case LLL_CLIENT:
        case LLL_LATENCY:
        case LLL_USER:
        case LLL_THREAD:
        default:
            Logger->trace(logLines);
            break;
    }
}

static inline const std::shared_ptr<spdlog::logger>& LwsLog()
{
    if(!Logger) {
#ifndef NDEBUG
        InitLwsLogger(spdlog::level::debug);
#else
        InitLwsLogger(spdlog::level::info);
#endif
    }

    return Logger;
}
