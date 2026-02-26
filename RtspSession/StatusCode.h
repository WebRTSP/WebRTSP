#pragma once


namespace rtsp {

enum StatusCode {
    OK = 200,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    SESSION_NOT_FOUND = 454,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
};

}
