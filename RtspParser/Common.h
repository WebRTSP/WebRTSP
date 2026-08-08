#pragma once

#include <string>
#include <set>
#include <map>


namespace rtsp {

typedef unsigned CSeq;
inline constexpr CSeq InvalidCSeq = 0;

typedef std::string MediaSessionId;

typedef std::map<std::string, std::string> Parameters;
typedef std::set<std::string> ParametersNames;

inline constexpr char UriSeparator = '/';
inline constexpr std::string_view WildcardUri = "*";

inline constexpr std::string_view AuthorizationFieldName = "Authorization";
inline constexpr std::string_view TextListContentType = "text/list";
inline constexpr std::string_view TextParametersContentType = "text/parameters";
inline constexpr std::string_view SdpContentType = "application/sdp";
inline constexpr std::string_view IceCandidateContentType = "application/x-ice-candidate";

}
