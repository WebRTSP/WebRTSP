#pragma once


namespace rtsp {

enum StatusCode {
    OK = 200,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    BAD_GATEWAY = 502,
};

}
