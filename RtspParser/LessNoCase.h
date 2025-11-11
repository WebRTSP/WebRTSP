#pragma once

#include <string>
#include <algorithm>


namespace rtsp {

struct LessNoCase
{
    bool operator()(const std::string& l, const std::string& r) const {
        return std::lexicographical_compare(
            l.begin(), l.end(),
            r.begin(), r.end(),
            [] (std::string::value_type l, std::string::value_type r) {
                return std::tolower(l) < std::tolower(r);
            });
    }
};

}
