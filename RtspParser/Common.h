#pragma once

#include <string>
#include <set>
#include <map>


namespace rtsp {

typedef unsigned CSeq;
typedef std::string MediaSessionId;

typedef std::map<std::string, std::string> Parameters;
typedef std::set<std::string> ParametersNames;

const char UriSeparator = '/';
const char *const WildcardUri = "*";

const char *const ContentTypeFieldName = "Content-Type";
const char *const TextListContentType = "text/list";
const char *const TextParametersContentType = "text/parameters";
const char *const SdpContentType = "application/sdp";
const char *const IceCandidateContentType = "application/x-ice-candidate";

}
