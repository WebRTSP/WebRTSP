#pragma once

#include "Common.h"
#include "Request.h"
#include "Response.h"
#include "Authentication.h"


namespace rtsp {

bool ParseCSeq(const std::string&, CSeq* out) noexcept;

bool ParseRequest(const char*, size_t, Request*) noexcept;
bool ParseResponse(const char*, size_t, Response*) noexcept;

bool IsRequest(const char*, size_t) noexcept;

bool ParseParameters(
    const std::string& body,
    Parameters*) noexcept;

bool ParseParametersNames(
    const std::string& body,
    ParametersNames*) noexcept;

std::set<rtsp::Method> ParseOptions(const Response&);

std::pair<Authentication, std::string> ParseAuthentication(const Request&);

std::pair<std::string, std::string> SplitUri(const std::string& uri);

}
