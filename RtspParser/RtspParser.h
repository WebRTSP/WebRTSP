#pragma once

#include "Request.h"
#include "Response.h"


namespace rtsp {

bool ParseRequest(const char*, size_t, Request*) noexcept;
bool ParseResponse(const char*, size_t, Response*) noexcept;

}
