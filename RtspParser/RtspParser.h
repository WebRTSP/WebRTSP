#pragma once

#include <set>
#include <map>

#include "Common.h"
#include "Request.h"
#include "Response.h"


namespace rtsp {

bool ParseCSeq(const std::string&, CSeq* out) noexcept;

bool ParseRequest(const char*, size_t, Request*) noexcept;
bool ParseResponse(const char*, size_t, Response*) noexcept;

bool IsRequest(const char*, size_t) noexcept;

typedef std::map<std::string, std::string> Parameters;
bool ParseParameters(
    const std::string& body,
    Parameters*) noexcept;

typedef std::set<std::string> ParametersNames;
bool ParseParametersNames(
    const std::string& body,
    ParametersNames*) noexcept;

std::set<rtsp::Method> ParseOptions(const Response&);

}
