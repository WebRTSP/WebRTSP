#pragma once


namespace GstStreaming
{

enum class Videocodec {
    h264,
    vp8
};

enum class IceServerType {
    Unknown,
    Stun,
    Turn,
    Turns,
};

}
