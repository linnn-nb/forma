#include <nativedaw/v2/EngineCommands.h>
#include <cmath>

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

void Commands::registerTransportCommands(Json& registry)
{
    const Json enabled{{"type", "boolean"}};
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
            {"loop_range", loopRange}};
}

Json Commands::validateTransportPlan(const Json& operations) const
{
    bool metronome = edit->clickTrackEnabled.get();
    bool loopEnabled = edit->getTransport().looping.get();
    auto mode = int(metadata.getProperty("count_in_mode", int(te::Edit::CountIn::none)));
    auto selection = timelineRange();
    auto loopRange = transportSettingsQuery().value("loop_range", Json(nullptr));
    Json diff = Json::array();
    for (const auto& operation : operations)
    {
        const auto command = operation.at("command").get<std::string>();
        const auto& args = operation.at("args");
        if (command == "session.range.set")
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

void Commands::restoreTransportSettings()
{
    const auto stored = int(metadata.getProperty("count_in_mode", int(te::Edit::CountIn::none)));
    if (encodeCountIn(stored) == "unsupported")
        edit->setCountInMode(te::Edit::CountIn::none);
    else
        edit->setCountInMode(static_cast<te::Edit::CountIn>(stored));
}
} // namespace ndaw::v2
