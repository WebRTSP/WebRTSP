#pragma once

#include <string>


namespace rtsp {

typedef unsigned CSeq;
typedef std::string MediaSessionId;

const char UriSeparator = '/';
const char *const WildcardUri = "*";

const char *const ContentTypeFieldName = "Content-Type";
const char *const TextParametersContentType = "text/parameters";
const char *const SdpContentType = "application/sdp";
const char *const IceCandidateContentType = "application/x-ice-candidate";

}
