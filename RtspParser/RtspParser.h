#pragma once

#include "Common.h"
#include "Request.h"
#include "Response.h"


namespace rtsp {

bool ParseCSeq(const std::string&, CSeq* out) noexcept;

bool ParseRequest(const char*, size_t, Request*) noexcept;
bool ParseResponse(const char*, size_t, Response*) noexcept;

bool IsRequest(const char*, size_t) noexcept;

}
