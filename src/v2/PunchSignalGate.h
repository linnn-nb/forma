// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "SelectionOutputGate.h"
namespace ndaw::v2
{
// Constructed on the message thread. The callback uses bounded audio spans only.
class PunchSignalGate final : public tracktion::graph::Node, public te::TracktionEngineNode
{
public:
    PunchSignalGate(std::unique_ptr<Node> source, te::ProcessState& process, double begin, double end, bool keepInside)
        : TracktionEngineNode(process), input(std::move(source)), beginSeconds(begin), endSeconds(end),
          inside(keepInside)
    {
        setOptimisations({tracktion::graph::ClearBuffers::no, tracktion::graph::AllocateAudioBuffer::yes});
    }
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        auto properties = input->getNodeProperties();
        properties.nodeID = 0;
        properties.hasMidi = false;
        return properties;
    }
    std::vector<Node*> getDirectInputNodes() override
    {
        return {input.get()};
    }
    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    void prepareToPlay(const tracktion::graph::PlaybackInitialisationInfo& info) override
    {
        first = std::llround(beginSeconds * info.sampleRate);
        last = std::llround(endSeconds * info.sampleRate);
        latency = input->getNodeProperties().latencyNumSamples;
    }
    void process(ProcessContext& pc) override
    {
        auto audio = input->getProcessedOutput().audio;
        if (!getPlayHead().isPlaying())
        {
            setAudioOutput(input.get(), audio);
            return;
        }
        tracktion::graph::copyIfNotAliased(pc.buffers.audio, audio);
        const auto position = getTimelineSampleRange().getStart() - latency;
        const int64_t frames = pc.buffers.audio.getNumFrames();
        const auto begin = std::clamp(first - position, int64_t(0), frames),
                   end = std::clamp(last - position, int64_t(0), frames);
        if (inside)
        {
            if (begin > 0)
                pc.buffers.audio.getStart(choc::buffer::FrameCount(begin)).clear();
            if (end < frames)
                pc.buffers.audio.fromFrame(choc::buffer::FrameCount(end)).clear();
        }
        else if (end > begin)
            pc.buffers.audio.getFrameRange({choc::buffer::FrameCount(begin), choc::buffer::FrameCount(end)}).clear();
    }

private:
    std::unique_ptr<Node> input;
    double beginSeconds, endSeconds;
    bool inside;
    int64_t first = 0, last = 0, latency = 0;
};
} // namespace ndaw::v2
