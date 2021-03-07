#include "GstStreamer.h"

#include <cassert>


void GstStreamer::prepare(
    const IceServers& iceServers,
    const PreparedCallback& prepared,
    const IceCandidateCallback& iceCandidate,
    const EosCallback& eos) noexcept
{
    assert(!pipeline());
    if(pipeline())
        return;

    _preparedCallback = prepared;
    _iceCandidateCallback = iceCandidate;
    _eosCallback = eos;

    PrepareResult pr = prepare();

    setPipeline(std::move(pr.pipelinePtr));
    setWebRtcBin(std::move(pr.webRtcBinPtr));

    setIceServers(iceServers);

    pause();
}

void GstStreamer::prepared()
{
    if(_preparedCallback)
        _preparedCallback();
}

void GstStreamer::iceCandidate(unsigned mlineIndex, const std::string& candidate)
{
    if(_iceCandidateCallback)
        _iceCandidateCallback(mlineIndex, candidate);
}

void GstStreamer::eos(bool /*error*/)
{
    if(_eosCallback)
        _eosCallback();
}
