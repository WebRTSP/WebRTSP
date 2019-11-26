#pragma once

#include <memory>
#include <functional>


class GstClient
{
public:
    GstClient();
    ~GstClient();

    typedef std::function<void ()> PreparedCallback;
    void prepare(const PreparedCallback&) noexcept;
    bool sdp(std::string* sdp) noexcept;

    void setRemoteSdp(const std::string& sdp) noexcept;
    void play() noexcept;

private:
    void eos(bool error);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};
