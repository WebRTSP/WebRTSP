#pragma once

#include <memory>

#include <spdlog/spdlog.h>


void InitWsServerLogger(spdlog::level::level_enum level);
const std::shared_ptr<spdlog::logger>& WsServerLog();

void InitServerSessionLogger(spdlog::level::level_enum level);
const std::shared_ptr<spdlog::logger>& ServerSessionLog();
