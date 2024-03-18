#pragma once

#include <string>


namespace rtsp {

typedef unsigned CSeq;
typedef std::string SessionId;

const char UriSeparator = '/';
const char *const WildcardUri = "*";
const char *const TextParametersContentType = "text/parameters";

}
