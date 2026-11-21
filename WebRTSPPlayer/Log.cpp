#include "Log.h"

#include "Helpers/SpdLog.h"

const std::shared_ptr<spdlog::logger>& WebRTSPPlayerLog() noexcept
{
    static std::shared_ptr<spdlog::logger> logger =
        CreateSpdLoggerSt("WebRTSPPlayer");

    return logger;
}
