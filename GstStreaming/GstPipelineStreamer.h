#pragma once

#include "Types.h"
#include "GstWebRTCPeer.h"


class GstPipelineStreamer : public GstWebRTCPeer
{
public:
    GstPipelineStreamer(const std::string& pipeline);

protected:
    void prepare() override;

private:
    const std::string _pipeline;
};
