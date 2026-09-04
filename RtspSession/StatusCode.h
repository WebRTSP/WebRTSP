#pragma once


namespace rtsp {

enum StatusCode {
    NONE = 0,
    OK = 200,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    NOT_ENOUGH_BANDWIDTH = 453,
    SESSION_NOT_FOUND = 454,
    INTERNAL_ERROR = 500,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
};

}
