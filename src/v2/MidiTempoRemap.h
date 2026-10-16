// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <json.hpp>
namespace ndaw::v2::midi_tempo
{
// L1 message-thread preparation only. No second Edit, playback graph or RT work.
class Snapshot
{
public:
    using Sequence = tracktion::tempo::Sequence;
    Snapshot(tracktion::engine::Edit&, const Sequence&);
    bool empty() const
    {
        return rows.empty();
    }
    nlohmann::json preview(const Sequence&) const;
    nlohmann::json automationPreview(const Sequence&) const;
    void apply(const Sequence&) const;

private:
    struct Row
    {
        juce::ValueTree live, original, parent, list;
        std::string track, playlist;
        bool active = false, locked = false;
        std::vector<juce::ValueTree> events;
        nlohmann::json controllerKeys;
    };
    struct Change
    {
        juce::ValueTree after;
        bool changed = false;
    };
    std::vector<Change> prepare(const Sequence&) const;
    struct Curve
    {
        juce::ValueTree live, original;
        std::string parameter;
        bool musical = false, remap = false, locked = false;
    };
    std::vector<juce::ValueTree> prepareCurves(const Sequence&) const;
    tracktion::engine::Edit& edit;
    Sequence before;
    std::string tempoHash;
    std::vector<Row> rows;
    std::vector<Curve> curves;
};
} // namespace ndaw::v2::midi_tempo
