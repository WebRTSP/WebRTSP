#pragma once

#include <string>


namespace rtsp {

typedef unsigned CSeq;
typedef std::string MediaSessionId;

const char UriSeparator = '/';
const char *const WildcardUri = "*";
const char *const TextParametersContentType = "text/parameters";

}
