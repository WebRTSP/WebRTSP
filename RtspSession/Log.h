#pragma once

#include <memory>

#include <spdlog/spdlog.h>


void InitRtspSessionLogger(spdlog::level::level_enum);
const std::shared_ptr<spdlog::logger>& RtspSessionLog();
std::shared_ptr<spdlog::logger> MakeRtspSessionLogger(const std::string& context);
