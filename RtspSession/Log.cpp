#include "Log.h"

#include "Helpers/SpdLog.h"


static std::shared_ptr<spdlog::logger> SessionLogger;

namespace rtsp {

void InitSessionLogger(spdlog::level::level_enum level) noexcept
{
    if(!SessionLogger)
        SessionLogger = CreateSpdLoggerSt("Session");

    SessionLogger->set_level(level);
}

const std::shared_ptr<spdlog::logger>& SessionLog() noexcept
{
    if(!SessionLogger) {
#ifdef NDEBUG
        InitSessionLogger(spdlog::level::info);
#else
        InitSessionLogger(spdlog::level::debug);
#endif
    }

    return SessionLogger;
}

}
