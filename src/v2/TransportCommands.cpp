#include <nativedaw/v2/EngineCommands.h>
#include <cmath>
#include <charconv>
#include "TimelineState.h"
#include "OutputProbe.h"
#include "SelectionOutputGate.h"

namespace ndaw::v2
{
namespace
{
te::Edit::CountIn decodeCountIn(const std::string& value)
{
    if (value == "one_beat")
        return te::Edit::CountIn::oneBeat;
    if (value == "two_beats")
        return te::Edit::CountIn::twoBeat;
    if (value == "one_bar")
        return te::Edit::CountIn::oneBar;
    if (value == "two_bars")
        return te::Edit::CountIn::twoBar;
    return te::Edit::CountIn::none;
}

std::string encodeCountIn(int value)
{
    switch (static_cast<te::Edit::CountIn>(value))
    {
    case te::Edit::CountIn::oneBeat:
        return "one_beat";
    case te::Edit::CountIn::twoBeat:
        return "two_beats";
    case te::Edit::CountIn::oneBar:
        return "one_bar";
    case te::Edit::CountIn::twoBar:
        return "two_bars";
    case te::Edit::CountIn::none:
        return "none";
    }
    return "unsupported";
}
} // namespace

Json readRollState(const juce::ValueTree& metadata)
{
    Json result{{"pre_enabled", false}, {"post_enabled", false}, {"pre_samples", 96000}, {"post_samples", 96000}};
    auto state = metadata.getChildWithName("ROLL");
    if (!state.isValid())
        return result;
    if (state.getNumProperties() != 5 || state.getNumChildren() != 0 || state.getProperty("schema").toString() != "1")
        throw std::runtime_error("invalid roll state schema");
    for (auto key : {"pre_enabled", "post_enabled", "pre_samples", "post_samples"})
    {
        if (!state.hasProperty(key))
            throw std::runtime_error("incomplete roll state");
        const auto value = state.getProperty(key).toString().toStdString();
        int64_t n = 0;
        auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), n);
        if (error != std::errc{} || end != value.data() + value.size() || n < 0 ||
            n > (std::string(key).ends_with("enabled") ? 1 : std::llround(te::Edit::maximumLength * 48000.)))
            throw std::runtime_error("invalid saved roll setting");
        if (std::string(key).ends_with("enabled"))
            result[key] = bool(n);
        else
            result[key] = n;
    }
    return result;
}

void Commands::registerTransportCommands(Json& registry)
{
    const Json enabled{{"type", "boolean"}};
    registry.push_back(Json{{"id", "transport.roll.set"},
                            {"schema",
                             {{"type", "object"},
                              {"properties",
                               {{"pre_enabled", enabled},
                                {"post_enabled", enabled},
                                {"pre_samples", {{"type", "integer"}, {"minimum", 0}}},
                                {"post_samples", {{"type", "integer"}, {"minimum", 0}}}}},
                              {"required", {"pre_enabled", "post_enabled", "pre_samples", "post_samples"}},
                              {"additionalProperties", false}}},
                            {"permission", "edit"},
                            {"risk", "low"},
                            {"reversible", true},
                            {"live", false},
                            {"tool_visibility", "local_gui"},
                            {"test", "U-P0-ROLL-01"},
                            {"units", {{"time", "48000 Hz session samples"}}}});

    Json clickSchema{{"type", "object"},
                     {"properties", {{"enabled", enabled}}},
                     {"required", {"enabled"}},
                     {"additionalProperties", false}};
    registry.push_back(Json{{"id", "transport.metronome.set"},
                            {"schema", clickSchema},
                            {"permission", "edit"},
                            {"risk", "low"},
                            {"reversible", true},
                            {"live", true},
                            {"test", "U-P0-TRANSPORT-01"}});

    Json modeSchema{
        {"type", "object"},
        {"properties",
         {{"mode", {{"type", "string"}, {"enum", {"none", "one_beat", "two_beats", "one_bar", "two_bars"}}}}}},
        {"required", {"mode"}},
        {"additionalProperties", false}};
    registry.push_back(Json{{"id", "transport.count_in.set"},
                            {"schema", modeSchema},
                            {"permission", "edit"},
                            {"risk", "low"},
                            {"reversible", true},
                            {"live", true},
                            {"test", "U-P0-TRANSPORT-01"}});

    registry.push_back(Json{{"id", "transport.loop.set"},
                            {"schema",
                             {{"type", "object"},
                              {"properties", {{"enabled", enabled}}},
                              {"required", {"enabled"}},
                              {"additionalProperties", false}}},
                            {"permission", "edit"},
                            {"risk", "low"},
                            {"reversible", true},
                            {"live", true},
                            {"test", "U-P0-LOOP-01"}});
}

Json Commands::transportSettingsQuery() const
{
    const auto mode = int(metadata.getProperty("count_in_mode", int(te::Edit::CountIn::none)));
    Json loopRange = nullptr;
    if (metadata.getProperty("loop_range_configured", false))
    {
        const auto nativeRange = edit->getTransport().getLoopRange();
        const auto start = int64_t(std::llround(nativeRange.getStart().inSeconds() * 48000.0));
        const auto end = int64_t(std::llround(nativeRange.getEnd().inSeconds() * 48000.0));
        if (start >= 0 && end > start)
            loopRange = {{"start_samples", start},
                         {"end_samples", end},
                         {"length_samples", end - start},
                         {"timebase", "session_samples"},
                         {"sample_rate", 48000}};
    }
    return {{"metronome_enabled", edit->clickTrackEnabled.get()},
            {"click_output", edit->getClickTrackDevice().toStdString()},
            {"count_in_mode", encodeCountIn(mode)},
            {"count_in_beats", encodeCountIn(mode) == "unsupported" ? 0 : edit->getNumCountInBeats()},
            {"count_in_source", "session_metadata"},
            {"loop_enabled", edit->getTransport().looping.get()},
            {"loop_range", loopRange},
            {"roll", readRollState(metadata)},
            {"roll_playback", rollPlayback},
            {"audio_gate_active", rollGate != nullptr && rollGate->enabled.load(std::memory_order_acquire)}};
}

Json Commands::validateTransportPlan(const Json& operations) const
{
    bool metronome = edit->clickTrackEnabled.get();
    bool loopEnabled = edit->getTransport().looping.get();
    auto mode = int(metadata.getProperty("count_in_mode", int(te::Edit::CountIn::none)));
    auto selection = timelineRange();
    auto loopRange = transportSettingsQuery().value("loop_range", Json(nullptr));
    auto roll = readRollState(metadata);
    Json diff = Json::array();
    for (const auto& operation : operations)
    {
        const auto command = operation.at("command").get<std::string>();
        const auto& args = operation.at("args");
        if (command == "transport.roll.set")
        {
            for (auto key : {"pre_samples", "post_samples"})
                if (args.at(key).get<int64_t>() < 0 ||
                    args.at(key).get<int64_t>() > std::llround(te::Edit::maximumLength * 48000.))
                    throw std::runtime_error("roll duration outside Edit range");
            diff.push_back({{"command", command}, {"before", roll}, {"after", args}});
            roll = args;
        }
        else if (command == "session.range.set")
        {
            const auto start = args.at("start_samples").get<int64_t>();
            const auto end = args.at("end_samples").get<int64_t>();
            if (start < 0 || end <= start)
                throw std::runtime_error("invalid time selection for loop playback");
            selection = {{"start_samples", start},
                         {"end_samples", end},
                         {"length_samples", end - start},
                         {"timebase", "session_samples"},
                         {"sample_rate", 48000}};
        }
        else if (command == "session.range.clear")
        {
            selection = nullptr;
        }
        else if (command == "transport.metronome.set")
        {
            const auto next = args.at("enabled").get<bool>();
            diff.push_back({{"command", command}, {"before", metronome}, {"after", next}});
            metronome = next;
        }
        else if (command == "transport.loop.set")
        {
            const auto next = args.at("enabled").get<bool>();
            const auto previousRange = loopRange;
            if (next && !selection.is_null())
                loopRange = selection;
            if (next && loopRange.is_null())
                throw std::runtime_error("select a time range before enabling loop playback");
            if (next && (loopRange.at("start_samples").get<int64_t>() < 0 ||
                         loopRange.at("end_samples").get<int64_t>() <= loopRange.at("start_samples").get<int64_t>()))
                throw std::runtime_error("invalid saved loop range");
            diff.push_back({{"command", command},
                            {"before", loopEnabled},
                            {"after", next},
                            {"before_range", previousRange},
                            {"after_range", loopRange}});
            loopEnabled = next;
        }
        else if (command == "transport.count_in.set")
        {
            const auto next = args.at("mode").get<std::string>();
            diff.push_back({{"command", command}, {"before", encodeCountIn(mode)}, {"after", next}});
            mode = int(decodeCountIn(next));
        }
    }
    return diff;
}

void Commands::executeTransportOperation(const std::string& command, const Json& args)
{
    auto& undo = edit->getUndoManager();
    if (command == "transport.roll.set")
    {
        auto state = metadata.getOrCreateChildWithName("ROLL", &undo);
        state.setProperty("schema", 1, &undo);
        for (auto key : {"pre_enabled", "post_enabled"})
            state.setProperty(key, args.at(key).get<bool>(), &undo);
        for (auto key : {"pre_samples", "post_samples"})
            state.setProperty(key, juce::int64(args.at(key).get<int64_t>()), &undo);
        return;
    }
    if (command == "transport.metronome.set")
    {
        auto click = edit->state.getOrCreateChildWithName(te::IDs::CLICKTRACK, &undo);
        click.setProperty(te::IDs::active, args.at("enabled").get<bool>(), &undo);
        return;
    }
    if (command == "transport.count_in.set")
    {
        const auto mode = args.at("mode").get<std::string>();
        metadata.setProperty("count_in_mode", int(decodeCountIn(mode)), &undo);
        // Tracktion stores this value globally. The session metadata above is the
        // authoritative, undoable state; this mirrors it into the native recorder.
        edit->setCountInMode(decodeCountIn(mode));
        return;
    }
    if (command == "transport.loop.set")
    {
        const auto enabled = args.at("enabled").get<bool>();
        auto& transport = edit->getTransport();
        if (enabled)
        {
            const auto selection = timelineRange();
            if (!selection.is_null())
            {
                const auto start = selection.at("start_samples").get<int64_t>();
                const auto end = selection.at("end_samples").get<int64_t>();
                auto state = transport.state;
                state.setProperty(te::IDs::loopPoint1, double(start) / 48000.0, &undo);
                state.setProperty(te::IDs::loopPoint2, double(end) / 48000.0, &undo);
                metadata.setProperty("loop_range_configured", true, &undo);
            }
            if (!metadata.getProperty("loop_range_configured", false))
                throw std::runtime_error("select a time range before enabling loop playback");
        }
        transport.state.setProperty(te::IDs::looping, enabled, &undo);
        return;
    }
    throw std::runtime_error("unknown transport operation");
}

bool Commands::beginRollPlayback()
{
    releaseRollGraph();
    const auto roll = readRollState(metadata);
    rollPlayback = nullptr;
    if (edit->getTransport().looping.get())
        return false; // Existing loop mode has explicit precedence.
    const auto range = timelineRange();
    if (range.is_null())
    {
        if (roll["pre_enabled"].get<bool>() || roll["post_enabled"].get<bool>())
            throw std::runtime_error("select a time range for pre/post-roll playback");
        return false;
    }
    const auto first = range["start_samples"].get<int64_t>(), last = range["end_samples"].get<int64_t>();
    const auto start =
        std::max(int64_t(0), first - (roll["pre_enabled"].get<bool>() ? roll["pre_samples"].get<int64_t>() : 0));
    const auto end = std::min(std::llround(te::Edit::maximumLength * 48000.),
                              last + (roll["post_enabled"].get<bool>() ? roll["post_samples"].get<int64_t>() : 0));
    const auto firstFrame = outputProbe ? outputProbe->frames.load(std::memory_order_relaxed) : 0;
    rollGate = std::make_shared<SelectionOutputGateState>();
    rollGate->startSeconds = start / 48000.;
    rollGate->endSeconds = end / 48000.;
    auto state = rollGate;
    edit->getTransport().prepareAuditionPlayback(
        tracktion::TimePosition::fromSeconds(start / 48000.),
        [state](te::EditPlaybackContext& context)
        {
            context.clearNodes();
            context.setInsertOptionalLastStageNodeForDeviceCallback(
                [state](te::OutputDevice& device, const te::CreateNodeParams& params,
                        std::unique_ptr<tracktion::graph::Node> input) -> std::unique_ptr<tracktion::graph::Node>
                {
                    if (dynamic_cast<te::WaveOutputDevice*>(&device))
                        return std::make_unique<SelectionOutputGate>(std::move(input), params.processState, state);
                    return input;
                });
        });
    edit->getTransport().playSectionAndReset(
        {tracktion::TimePosition::fromSeconds(start / 48000.), tracktion::TimePosition::fromSeconds(end / 48000.)});
    rollPlayback = {
        {"state", "requested"},
        {"frames_at_start", firstFrame},
        {"start_samples", start},
        {"end_samples", end},
        {"selection", range},
        {"stop_accuracy", "wave audio gated at nearest device sample; native transport stops on message thread"}};
    rollProgressFrames = firstFrame;
    rollProgressTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz(20);
    return true;
}
void Commands::releaseRollGraph()
{
    if (!rollGate)
        return;
    rollGate->enabled.store(false, std::memory_order_release);
    if (auto* context = edit->getTransport().getCurrentPlaybackContext())
        context->setInsertOptionalLastStageNodeForDeviceCallback({});
    rollGate.reset();
}
void Commands::advanceRollPlayback()
{
    if (rollPlayback.is_null() || (rollPlayback["state"] != "playing" && rollPlayback["state"] != "requested"))
        return;
    if (rollGate)
        rollPlayback["audio_boundary"] = {
            {"reached", rollGate->reached.load(std::memory_order_acquire)},
            {"processed_blocks", rollGate->processedBlocks.load(std::memory_order_relaxed)},
            {"end_samples", rollPlayback["end_samples"]},
            {"external_midi", "not gated; message-thread native stop"}};
    const auto frames = outputProbe ? outputProbe->frames.load(std::memory_order_relaxed) : 0;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto* context = edit->getTransport().getCurrentPlaybackContext();
    if (frames > rollPlayback["frames_at_start"].get<uint64_t>() && context && context->isPlaying())
        rollPlayback["state"] = "playing";
    if (frames != rollProgressFrames)
    {
        rollProgressFrames = frames;
        rollProgressTime = now;
    }
    if (edit->getTransport().isPlaying())
    {
        if (now - rollProgressTime <= 2000.)
            return;
        rollPlayback["state"] = "failed";
        rollPlayback["error"] = "output callback made no progress for two seconds";
    }
    else
    {
        const auto position = std::llround(edit->getTransport().getPosition().inSeconds() * 48000.);
        rollPlayback["actual_stop_samples"] = position;
        rollPlayback["overshoot_samples"] = std::max(int64_t(0), position - rollPlayback["end_samples"].get<int64_t>());
        rollPlayback["state"] = frames > rollPlayback["frames_at_start"].get<uint64_t>() &&
                                        position >= rollPlayback["end_samples"].get<int64_t>()
                                    ? "stopped"
                                    : "interrupted";
    }
    if (rollPlayback["state"] == "interrupted")
        rollPlayback["error"] = "native transport stopped before the selected end";
    // Keep the completed range mask while the SDK still drains its graph.
    // An explicit Stop/Seek/Play/Record retires it and restores normal monitoring.
    stopTransport(rollPlayback["state"] == "stopped");
}

void Commands::restoreTransportSettings()
{
    const auto stored = int(metadata.getProperty("count_in_mode", int(te::Edit::CountIn::none)));
    if (encodeCountIn(stored) == "unsupported")
        edit->setCountInMode(te::Edit::CountIn::none);
    else
        edit->setCountInMode(static_cast<te::Edit::CountIn>(stored));
}
} // namespace ndaw::v2
