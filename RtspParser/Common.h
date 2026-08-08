#pragma once

#include <string>
#include <set>
#include <map>
#include <algorithm>


namespace rtsp {

typedef unsigned CSeq;
inline constexpr CSeq InvalidCSeq = 0;

typedef std::string MediaSessionId;

typedef std::map<std::string, std::string> Parameters;
typedef std::set<std::string> ParametersNames;

inline constexpr char UriSeparator = '/';
inline constexpr std::string_view WildcardUri = "*";

inline constexpr std::string_view CSeqFieldName = "CSeq";
inline constexpr std::string_view SessionFieldName = "Session";
inline constexpr std::string_view ContentTypeFieldName = "Content-Type";
inline constexpr std::string_view AuthorizationFieldName = "Authorization";
inline constexpr std::string_view TextListContentType = "text/list";
inline constexpr std::string_view TextParametersContentType = "text/parameters";
inline constexpr std::string_view SdpContentType = "application/sdp";
inline constexpr std::string_view IceCandidateContentType = "application/x-ice-candidate";

constexpr std::string ToLower(std::string_view in)
{
    std::string out(in);
    std::transform(
        out.begin(),
        out.end(),
        out.begin(),
        [] (std::string::value_type c) {
            return c + (c >= 'A' && c <= 'Z' ? 'a' - 'A' : 0);
        });
    return out;
}

}
