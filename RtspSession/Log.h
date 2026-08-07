#pragma once

#include <memory>

#include <spdlog/spdlog.h>


namespace rtsp {

void InitSessionLogger(spdlog::level::level_enum) noexcept;
const std::shared_ptr<spdlog::logger>& SessionLog() noexcept;

}
