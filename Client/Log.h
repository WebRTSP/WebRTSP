#pragma once

#include <memory>

#include <spdlog/spdlog.h>


void InitWsClientLogger(spdlog::level::level_enum level);
const std::shared_ptr<spdlog::logger>& WsClientLog();

void InitClientSessionLogger(spdlog::level::level_enum level);
const std::shared_ptr<spdlog::logger>& ClientSessionLog();
