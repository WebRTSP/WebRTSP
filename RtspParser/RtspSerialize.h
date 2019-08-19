#pragma once

#include "Response.h"


namespace rtsp {

void Serialize(const Response&, std::string* out) noexcept;
std::string Serialize(const Response&) noexcept;

}
