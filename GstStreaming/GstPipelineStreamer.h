#pragma once

#include "Types.h"
#include "GstStreamer.h"


class GstPipelineStreamer : public GstStreamer
{
public:
    GstPipelineStreamer(const std::string& pipeline);

protected:
    PrepareResult prepare() override;

private:
    const std::string _pipeline;
};
