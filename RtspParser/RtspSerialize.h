#pragma once

#include "Request.h"
#include "Response.h"


namespace rtsp {

void Serialize(const Request&, std::string* out) noexcept;
std::string Serialize(const Request&) noexcept;

void Serialize(const Response&, std::string* out) noexcept;
std::string Serialize(const Response&) noexcept;

}
