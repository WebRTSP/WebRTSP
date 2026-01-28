#pragma once

#include <memory>

#include <spdlog/spdlog.h>


void InitRtspSessionLogger(spdlog::level::level_enum);
const std::shared_ptr<spdlog::logger>& RtspSessionLog();
std::shared_ptr<spdlog::logger> MakeRtspSessionLogger(const std::string& context);

void InitServerSessionLogger(spdlog::level::level_enum level);
std::shared_ptr<spdlog::logger> MakeServerSessionLogger(const std::string& context);

void InitClientSessionLogger(spdlog::level::level_enum level);
const std::shared_ptr<spdlog::logger>& ClientSessionLog();
std::shared_ptr<spdlog::logger> MakeClientSessionLogger(const std::string& context);

