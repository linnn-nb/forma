#include <nativedaw/v2/EngineCommands.h>
#include <limits>

namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
std::string id(const juce::ValueTree& state)
{
    return te::EditItemID::fromID(state).toString().toStdString();
}
tracktion::TimePosition time(int64_t samples)
{
    return tracktion::TimePosition::fromSeconds(samples / 48000.0);
}
int64_t samples(tracktion::TimePosition time)
{
    const auto value = time.inSeconds() * 48000;
    require(std::isfinite(value) && value >= 0 && value < double(std::numeric_limits<int64_t>::max()),
            "musical position exceeds sample representation");
    return std::llround(value);
}
int64_t sourceSamples(tracktion::TimePosition time)
{
    const double value = time.inSeconds() * 48000.;
    require(std::isfinite(value) && value > double(std::numeric_limits<int64_t>::min()) &&
                value < double(std::numeric_limits<int64_t>::max()),
            "musical source position exceeds signed sample representation");
    return std::llround(value);
}
void range(const Json& a)
{
    auto start = a.at("position_samples").get<int64_t>(), length = a.at("length_samples").get<int64_t>();
    require(start >= 0 && length > 0 && start <= std::numeric_limits<int64_t>::max() - length,
            "invalid musical sample range");
}
struct NoteModel
{
    double source = 0, length = 0;
    int pitch = 0, velocity = 0;
    bool muted = false;
};
struct ClipModel
{
    double start = 0, end = 0, content = 0;
    int64_t absoluteStart = 0, absoluteEnd = 0, absoluteContent = 0;
    bool beats = true, looped = false;
    std::map<std::string, NoteModel> notes;
    bool playbackProcessed = false, locked = false;
};
std::string transformRestriction(te::MidiClip& c)
{
    if (c.isLooping())
        return "looped";
    if (c.getQuantisation().getType(false) != "(none)")
        return "playback_quantisation";
    if (c.getGrooveTemplate().isNotEmpty())
        return "playback_groove";
    return "";
}
template <class Sequence> Json noteFacts(const NoteModel& n, const ClipModel& c, const Sequence& seq)
{
    const auto begin = seq.toTime(tracktion::BeatPosition::fromBeats(c.content + n.source)),
               end = seq.toTime(tracktion::BeatPosition::fromBeats(c.content + n.source + n.length));
    return {{"pitch", n.pitch},
            {"velocity", n.velocity},
            {"muted", n.muted},
            {"source_beat", n.source},
            {"length_beats", n.length},
            {"start_beat", c.content + n.source},
            {"position_samples", sourceSamples(begin)},
            {"length_samples", sourceSamples(end) - sourceSamples(begin)}};
}
template <class Sequence> std::vector<std::string> selectNotes(const Json& a, const ClipModel& c, const Sequence& seq)
{
    require(!c.looped, "bulk MIDI transformation of looped clips is not qualified");
    require(!c.playbackProcessed,
            "MIDI clip has playback quantisation or groove; source transformation is not qualified");
    const std::string selection = a.at("selection");
    require(selection == "all" || selection == "notes" || selection == "range", "invalid MIDI selection");
    require((selection == "notes") == a.contains("note_ids"), "note_ids required only for notes selection");
    require((selection == "range") == a.contains("range_start_samples") &&
                (selection == "range") == a.contains("range_end_samples"),
            "range endpoints required only for range selection");
    std::vector<std::string> ids;
    if (selection == "notes")
    {
        const auto& input = a.at("note_ids");
        require(input.is_array() && !input.empty(), "empty MIDI note selection");
        std::set<std::string> seen;
        for (const auto& value : input)
        {
            require(value.is_string(), "note ID must be a string");
            auto key = value.get<std::string>();
            require(seen.insert(key).second, "duplicate selected note ID");
            require(c.notes.contains(key), "selected MIDI note not found or removed earlier in Plan");
            ids.push_back(key);
        }
    }
    else
    {
        int64_t first = 0, last = std::llround(te::Edit::maximumLength * 48000);
        if (selection == "range")
        {
            first = a.at("range_start_samples");
            last = a.at("range_end_samples");
            require(first >= 0 && last > first && last <= std::llround(te::Edit::maximumLength * 48000),
                    "invalid MIDI half-open sample range");
        }
        for (const auto& [key, n] : c.notes)
        {
            const double onset = c.content + n.source;
            if (onset < c.start - 1e-8 || onset >= c.end)
                continue;
            const auto position = samples(seq.toTime(tracktion::BeatPosition::fromBeats(onset)));
            if (position >= first && position < last)
                ids.push_back(key);
        }
    }
    require(!ids.empty(), "no MIDI note onsets in the requested selection");
    return ids;
}
NoteModel transformNote(const std::string& cmd, const Json& a, const ClipModel& c, NoteModel n)
{
    if (cmd == "midi.notes.transpose")
    {
        const int interval = a.at("semitones");
        require(interval >= -127 && interval <= 127, "transpose outside -127..127 semitones");
        require(n.pitch + interval >= 0 && n.pitch + interval <= 127,
                "transpose would exceed MIDI pitch 0..127; entire Plan rejected");
        n.pitch += interval;
    }
    else
    {
        const double grid = a.at("grid_beats"), strength = a.at("strength");
        require(std::isfinite(grid) && grid >= 1. / 128 && grid <= 32, "quantise grid outside 1/128..32 meter beats");
        require(std::isfinite(strength) && strength >= 0 && strength <= 1, "quantise strength outside 0..1");
        const double onset = c.content + n.source, target = std::floor(onset / grid + .5) * grid;
        const double changed = onset + (target - onset) * strength;
        require(changed >= c.start - 1e-8 && changed >= c.content - 1e-8 && changed + n.length <= c.end + 1e-8,
                "quantise would move a note outside its playable clip; entire Plan rejected");
        n.source = std::max(0., changed - c.content);
    }
    return n;
}
template <class Sequence>
NoteModel transformTimingNote(const Json& a, const ClipModel& c, NoteModel n, const Sequence& seq)
{
    require(!c.locked, "MIDI clip is locked");
    const std::string edge = a.at("edge"), unit = a.at("unit");
    const double amount = a.at("amount");
    require(std::isfinite(amount) && amount != 0, "empty or invalid MIDI timing offset");
    require(unit == "beats" || (unit == "samples" && std::trunc(amount) == amount), "sample offset must be integral");
    double first = c.content + n.source, last = first + n.length;
    if (unit == "beats")
    {
        if (edge != "end")
            first += amount;
        if (edge != "start")
            last += amount;
    }
    else
    {
        auto shifted = [&](double beat)
        {
            const double seconds = seq.toTime(tracktion::BeatPosition::fromBeats(beat)).inSeconds() + amount / 48000.;
            require(seconds >= 0, "MIDI timing offset crosses session start");
            return seq.toBeats(tracktion::TimePosition::fromSeconds(seconds)).inBeats();
        };
        if (edge != "end")
            first = shifted(first);
        if (edge != "start")
            last = shifted(last);
    }
    require(first >= c.start && first >= c.content && last <= c.end && last > first,
            "MIDI timing edit exceeds playable clip or inverts a note; entire Plan rejected");
    require(samples(seq.toTime(tracktion::BeatPosition::fromBeats(last))) >
                samples(seq.toTime(tracktion::BeatPosition::fromBeats(first))),
            "MIDI note shorter than one session sample");
    if (edge != "end")
        n.source = first - c.content;
    // Musical moves preserve the original source duration exactly, including
    // imported sub-sample beats. Trims retain the opposite project beat edge.
    if (!(edge == "move" && unit == "beats"))
        n.length = last - first;
    return n;
}
} // namespace
void Commands::registerMusicCommands(Json& registry)
{
    auto add = [&](const char* command, Json properties)
    {
        Json required = Json::array();
        for (auto i = properties.begin(); i != properties.end(); ++i)
            required.push_back(i.key());
        registry.push_back({{"id", command},
                            {"schema",
                             {{"type", "object"},
                              {"properties", properties},
                              {"required", required},
                              {"additionalProperties", false}}},
                            {"permission", "edit"},
                            {"risk", "low"},
                            {"reversible", true},
                            {"live", false},
                            {"test", "M1-MIDI-01"}});
        registry.back()["units"] = {
            {"position_samples", "absolute session samples at 48000 Hz, converted with the current Tempo map"},
            {"length_samples", "duration at the current Tempo map; MIDI stores musical beats"}};
    };
    Json str = {{"type", "string"}}, integer = {{"type", "integer"}}, position = {{"type", "integer"}, {"minimum", 0}},
         length = {{"type", "integer"}, {"minimum", 1}};
    add("tempo.set",
        {{"position_samples", position},
         {"bpm", {{"type", "number"}, {"minimum", te::TempoSetting::minBPM}, {"maximum", te::TempoSetting::maxBPM}}}});
    add("meter.set", {{"position_samples", position}, {"numerator", integer}, {"denominator", integer}});
    registry.back()["units"]["position_samples"] = "bar boundary in the current Tempo/meter map";
    for (const auto* kind : {"tempo", "meter"})
        for (const auto* action : {"create", "set", "delete"})
        {
            Json args = Json::object();
            if (std::string(action) != "create")
                args["event"] = str;
            if (std::string(action) != "delete")
            {
                args["beat_position"] = {{"type", "number"}, {"minimum", 0}};
                if (std::string(kind) == "tempo")
                    args["bpm"] = {{"type", "number"}, {"minimum", 20}, {"maximum", 300}};
                else
                {
                    args["numerator"] = integer;
                    args["denominator"] = integer;
                }
            }
            add((std::string(kind) + ".event." + action).c_str(), args);
            registry.back()["tool_visibility"] = "local_gui";
            registry.back()["test"] = "U-P0-MUSIC-EVENTS-01";
            registry.back()["units"] = {
                {"beat_position",
                 "absolute Tracktion meter divisions, zero based; denominator changes division duration"}};
        }
    add("midi.clip.create",
        {{"track", str}, {"ref", str}, {"name", str}, {"position_samples", position}, {"length_samples", length}});
    Json note = {{"clip", str},
                 {"pitch", {{"type", "integer"}, {"minimum", 0}, {"maximum", 127}}},
                 {"velocity", {{"type", "integer"}, {"minimum", 1}, {"maximum", 127}}},
                 {"position_samples", position},
                 {"length_samples", length}};
    add("midi.note.add", note);
    registry.back()["schema"]["properties"]["ref"] = str;
    note["note"] = str;
    add("midi.note.set", note);
    add("midi.note.delete", {{"clip", str}, {"note", str}});
    Json selection = {{"clip", str}, {"selection", {{"type", "string"}, {"enum", {"all", "notes", "range"}}}}};
    auto transform = [&](const char* name, Json parameters)
    {
        parameters.update(selection);
        add(name, parameters);
        auto& r = registry.back();
        r["test"] = "M1-MIDI-TRANSFORM-01";
        r["schema"]["properties"]["note_ids"] = {
            {"type", "array"}, {"items", str}, {"minItems", 1}, {"uniqueItems", true}};
        r["schema"]["properties"]["range_start_samples"] = position;
        r["schema"]["properties"]["range_end_samples"] = position;
        r["units"].update(
            {{"grid_beats",
              "absolute session grid in Tracktion meter divisions; ties forward; preserves source beat duration"},
             {"strength", "0..1 proportion of onset displacement"},
             {"semitones", "integer MIDI semitones; out-of-range pitches reject the whole Plan"},
             {"selection", "all playable onsets, explicit stable IDs, or half-open session sample onset range"}});
    };
    transform("midi.notes.quantize", {{"grid_beats", {{"type", "number"}, {"minimum", 1. / 128}, {"maximum", 32}}},
                                      {"strength", {{"type", "number"}, {"minimum", 0}, {"maximum", 1}}}});
    transform("midi.notes.transpose", {{"semitones", {{"type", "integer"}, {"minimum", -127}, {"maximum", 127}}}});
    transform("midi.notes.time", {{"edge", {{"type", "string"}, {"enum", {"move", "start", "end"}}}},
                                  {"unit", {{"type", "string"}, {"enum", {"samples", "beats"}}}},
                                  {"amount", {{"type", "number"}, {"minimum", -2880000}, {"maximum", 2880000}}}});
    auto& timing = registry.back();
    timing["tool_visibility"] = "local_gui";
    timing["test"] = "U-P0-MIDI-TIME-01";
    timing["schema"]["properties"]["selection"]["enum"] = {"notes"};
    timing["schema"]["properties"]["note_ids"]["maxItems"] = 4096;
    timing["schema"]["required"].push_back("note_ids");
    timing["schema"]["properties"].erase("range_start_samples");
    timing["schema"]["properties"].erase("range_end_samples");
    timing["units"] = {
        {"unit", "48000 Hz session samples, or native Tracktion meter divisions"},
        {"amount", "signed edge displacement; samples integral; beats preserve source duration on move"}};
    add("midi.clips.erase", {{"clipboard", str},
                             {"ripple", {{"type", "boolean"}}},
                             {"state_hash", str},
                             {"ripple_mapping", {{"type", "string"}, {"enum", {"samples", "native"}}}}});
    registry.back()["schema"]["required"] = {"clipboard"};
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-MIDI-CLIPS-01";
    add("timeline.clips.erase", {{"clipboard", str},
                                 {"ripple", {{"type", "boolean"}}},
                                 {"state_hash", str},
                                 {"ripple_mapping", {{"type", "string"}, {"enum", {"samples", "native"}}}}});
    registry.back()["schema"]["required"] = {"clipboard"};
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-MIXED-CLIPBOARD-01";
    add("timeline.clips.paste", {{"clipboard", str},
                                 {"tracks", {{"type", "array"}, {"items", str}, {"maxItems", 64}}},
                                 {"position_samples", position},
                                 {"removal_end_samples", position},
                                 {"state_hash", str},
                                 {"ripple_mapping", {{"type", "string"}, {"enum", {"samples", "native"}}}},
                                 {"mode", {{"type", "string"}, {"enum", {"replace", "overlay", "shuffle"}}}}});
    registry.back()["schema"]["required"] = {"clipboard", "tracks", "position_samples", "mode"};
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-MIXED-CLIPBOARD-01";
    add("midi.clips.paste", {{"clipboard", str},
                             {"tracks", {{"type", "array"}, {"items", str}, {"maxItems", 64}}},
                             {"position_samples", position},
                             {"removal_end_samples", position},
                             {"state_hash", str},
                             {"ripple_mapping", {{"type", "string"}, {"enum", {"samples", "native"}}}},
                             {"mode", {{"type", "string"}, {"enum", {"replace", "overlay", "shuffle"}}}}});
    registry.back()["schema"]["required"] = {"clipboard", "tracks", "position_samples", "mode"};
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-MIDI-CLIPS-01";
    add("midi.notes.erase", {{"clip", str}, {"note_ids", {{"type", "array"}, {"items", str}, {"maxItems", 4096}}}});
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-MIDI-CLIPBOARD-01";
    add("midi.notes.paste", {{"clip", str},
                             {"clipboard", str},
                             {"position_samples", position},
                             {"placement", {{"type", "string"}, {"enum", {"cursor", "original", "after"}}}},
                             {"mode", {{"type", "string"}, {"enum", {"replace", "merge"}}}}});
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-MIDI-CLIPBOARD-01";
}
std::string Commands::trackType(te::AudioTrack& t) const
{
    if (t.pluginList.findFirstPluginOfType<te::AuxReturnPlugin>())
        return "aux";
    if (t.pluginList.findFirstPluginOfType<te::FourOscPlugin>())
        return "instrument";
    for (auto* p : t.pluginList)
        if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p); ext && ext->desc.isInstrument)
            return "instrument";
    const auto role = t.state.getProperty("ndaw_role").toString();
    if (role == "midi" || role == "instrument")
        return role.toStdString();
    return "audio";
}
void Commands::createMusicTrack(te::AudioTrack& t, const std::string& type, Json& objects)
{
    t.state.setProperty("ndaw_role", juce::String(type), &edit->getUndoManager());
    if (type == "instrument")
        executeProcessorOperation(
            "plugin.insert", {{"track", t.itemID.toString().toStdString()}, {"type", te::FourOscPlugin::xmlTypeName}},
            objects);
}
te::MidiClip* Commands::midiClip(const std::string& target) const
{
    for (auto* t : te::getAudioTracks(*edit))
        for (auto* c : t->getClips())
            if (c->itemID.toString().toStdString() == target)
                return dynamic_cast<te::MidiClip*>(c);
    return nullptr;
}
void Commands::initialiseMusicIDs(juce::UndoManager* undo)
{
    std::set<std::string> seen;
    auto repairs = Json::parse(metadata.getProperty("music_id_repairs", "[]").toString().toStdString());
    auto ensure = [&](const juce::ValueTree& value, const char* kind, double beat)
    {
        auto state = value;
        const auto previous = id(state);
        const bool duplicate = !te::EditItemID::fromID(state).isInvalid() && seen.contains(previous);
        if (te::EditItemID::fromID(state).isInvalid() || duplicate)
        {
            edit->createNewItemID().writeID(state, undo);
            if (duplicate)
                repairs.push_back(
                    {{"kind", kind}, {"previous_id", previous}, {"replacement_id", id(state)}, {"start_beat", beat}});
        }
        seen.insert(id(state));
    };
    for (auto* t : edit->tempoSequence.getTempos())
        ensure(t->state, "tempo", t->getStartBeat().inBeats());
    for (auto* t : edit->tempoSequence.getTimeSigs())
        ensure(t->state, "meter", t->getStartBeat().inBeats());
    if (!repairs.empty())
        metadata.setProperty("music_id_repairs", juce::String(repairs.dump()), undo);
    for (auto* t : te::getAudioTracks(*edit))
        for (auto* c : t->getClips())
            if (auto* m = dynamic_cast<te::MidiClip*>(c))
                for (auto* n : m->getSequence().getNotes())
                    if (te::EditItemID::fromID(n->state).isInvalid())
                        edit->createNewItemID().writeID(n->state, undo);
}
Json Commands::midiQuery(te::MidiClip& clip) const
{
    Json notes = Json::array();
    for (auto* n : clip.getSequence().getNotes())
    {
        auto begin = n->getEditStartTime(clip), end = n->getEditEndTime(clip);
        notes.push_back({{"id", id(n->state)},
                         {"pitch", n->getNoteNumber()},
                         {"velocity", n->getVelocity()},
                         {"muted", n->isMute()},
                         {"source_beat", n->getStartBeat().inBeats()},
                         {"length_beats", n->getLengthBeats().inBeats()},
                         {"start_beat", edit->tempoSequence.toBeats(begin).inBeats()},
                         {"position_samples", sourceSamples(begin)},
                         {"length_samples", sourceSamples(end) - sourceSamples(begin)}});
    }
    Json controllers = Json::array();
    for (auto* e : clip.getSequence().getControllerEvents())
        controllers.push_back({{"type", e->getType()},
                               {"raw_value", e->getControllerValue()},
                               {"metadata", e->getMetadata()},
                               {"source_beat", e->getBeatPosition().inBeats()},
                               {"position_samples", sourceSamples(e->getEditTime(clip))}});
    return {{"notes", notes},
            {"controller_events", controllers},
            {"sysex_count", clip.getSequence().getNumSysExEvents()},
            {"start_beat", clip.getStartBeat().inBeats()},
            {"length_beats", clip.getLengthInBeats().inBeats()},
            {"content_start_beat", clip.getContentStartBeat().inBeats()},
            {"midi_channel", clip.getMidiChannel().getChannelNumber()},
            {"looped", clip.isLooping()},
            {"locked", bool(clip.state.getProperty("ndaw_locked", false))},
            {"bulk_transform_available", transformRestriction(clip).empty()},
            {"bulk_transform_restriction", transformRestriction(clip)}};
}
Json Commands::musicQuery() const
{
    auto& seq = edit->tempoSequence;
    Json tempos = Json::array(), meters = Json::array();
    for (auto* t : seq.getTempos())
        tempos.push_back({{"id", id(t->state)},
                          {"start_beat", t->getStartBeat().inBeats()},
                          {"position_samples", samples(t->getStartTime())},
                          {"bpm", t->getBpm()},
                          {"curve", t->getCurve()}});
    for (auto* s : seq.getTimeSigs())
        meters.push_back({{"id", id(s->state)},
                          {"start_beat", s->getStartBeat().inBeats()},
                          {"position_samples", samples(seq.toTime(s->getStartBeat()))},
                          {"numerator", s->numerator.get()},
                          {"denominator", s->denominator.get()}});
    const auto now = edit->getTransport().getPosition();
    auto bb = seq.toBarsAndBeats(now);
    auto& sig = seq.getTimeSigAt(now);
    return {{"id_repairs", Json::parse(metadata.getProperty("music_id_repairs", "[]").toString().toStdString())},
            {"tempos", tempos},
            {"meters", meters},
            {"bpm", seq.getBpmAt(now)},
            {"numerator", sig.numerator.get()},
            {"denominator", sig.denominator.get()},
            {"bar", bb.bars + 1},
            {"beat", bb.beats.inBeats() + 1},
            {"position_beats", seq.toBeats(now).inBeats()},
            {"beat_unit", "Tracktion meter division; denominator changes beat duration"}};
}
int64_t Commands::sampleAtBeat(double beat) const
{
    checkThread();
    require(std::isfinite(beat) && beat >= 0, "invalid beat position");
    return samples(edit->tempoSequence.toTime(tracktion::BeatPosition::fromBeats(beat)));
}
int64_t Commands::sampleAtBarBeat(int bar, double beat) const
{
    checkThread();
    require(bar >= 1 && bar <= 1000000 && std::isfinite(beat) && beat >= 1.0 && beat < 65.0,
            "invalid bar and beat position");
    const auto target = edit->tempoSequence.toTime(
        tracktion::tempo::BarsAndBeats{.bars = bar - 1, .beats = tracktion::BeatDuration::fromBeats(beat - 1.0)});
    const auto roundTrip = edit->tempoSequence.toBarsAndBeats(target);
    require(roundTrip.bars == bar - 1 && std::abs(roundTrip.beats.inBeats() - (beat - 1.0)) < 1e-6,
            "beat is outside the selected bar in the current Tempo and Meter map");
    const auto result = samples(target);
    require(result >= 0 && result <= std::llround(te::Edit::maximumLength * timelineRate),
            "bar and beat position exceeds the session timeline");
    return result;
}
Json Commands::timelinePosition(int64_t position) const
{
    checkThread();
    require(position >= 0 && position <= std::llround(te::Edit::maximumLength * timelineRate),
            "timeline position outside Edit range");
    auto& seq = edit->tempoSequence;
    const auto now = time(position);
    const auto bb = seq.toBarsAndBeats(now);
    auto& meter = seq.getTimeSigAt(now);
    return {{"position_samples", position},
            {"bar", bb.bars + 1},
            {"beat", bb.beats.inBeats() + 1},
            {"position_beats", seq.toBeats(now).inBeats()},
            {"bpm", seq.getBpmAt(now)},
            {"numerator", meter.numerator.get()},
            {"denominator", meter.denominator.get()}};
}
Json Commands::musicalGrid(int64_t start, int64_t end, double division) const
{
    checkThread();
    require(start >= 0 && end > start && std::isfinite(division) && division > 0, "invalid grid range");
    auto& seq = edit->tempoSequence;
    double first = std::floor(seq.toBeats(time(start)).inBeats() / division) * division,
           last = seq.toBeats(time(end)).inBeats();
    // Viewport query budget, not a project length limit; coarsen long visible ranges.
    const double step = division * std::max(1., std::ceil((last - first) / division / 4095));
    Json grid = Json::array();
    for (double beat = first; beat <= last && grid.size() < 4096; beat += step)
    {
        auto t = seq.toTime(tracktion::BeatPosition::fromBeats(beat));
        auto bb = seq.toBarsAndBeats(t);
        grid.push_back({{"samples", samples(t)},
                        {"beat", beat},
                        {"bar", bb.bars + 1},
                        {"beat_in_bar", bb.beats.inBeats() + 1},
                        {"bar_line", std::abs(bb.beats.inBeats()) < 1e-6}});
    }
    return grid;
}
int64_t Commands::snapToGrid(int64_t position, double division) const
{
    checkThread();
    const auto maximum = std::llround(te::Edit::maximumLength * 48000);
    require(position >= 0 && position <= maximum && std::isfinite(division) && division >= 1. / 128 && division <= 32,
            "invalid editing grid position or division");
    const auto beat = edit->tempoSequence.toBeats(time(position)).inBeats();
    const auto lowerBeat = std::floor(beat / division) * division;
    const auto lower = sampleAtBeat(lowerBeat), upper = sampleAtBeat(lowerBeat + division);
    return std::clamp(position - lower < upper - position ? lower : upper, int64_t(0), maximum);
}
int64_t Commands::offsetByBeats(int64_t position, double beats) const
{
    checkThread();
    const auto maximum = std::llround(te::Edit::maximumLength * 48000);
    require(position >= 0 && position <= maximum && std::isfinite(beats) && std::abs(beats) <= 32,
            "invalid musical nudge");
    const auto target = edit->tempoSequence.toBeats(time(position)).inBeats() + beats;
    require(target >= 0, "musical nudge would cross session start");
    const auto result = sampleAtBeat(target);
    require(result <= maximum, "musical nudge exceeds session length");
    return result;
}
Json Commands::validateMusicPlan(const Json& operations) const
{
    using Sequence = tracktion::tempo::Sequence;
    using BP = tracktion::BeatPosition;
    std::vector<tracktion::tempo::TempoChange> tempos;
    std::vector<tracktion::tempo::TimeSigChange> meters;
    std::map<std::string, BP> tempoIDs, meterIDs;
    for (auto* t : edit->tempoSequence.getTempos())
        tempoIDs[id(t->state)] = t->getStartBeat();
    for (auto* t : edit->tempoSequence.getTimeSigs())
        meterIDs[id(t->state)] = t->getStartBeat();
    for (auto* t : edit->tempoSequence.getTempos())
        tempos.push_back({t->getStartBeat(), t->getBpm(), t->getCurve()});
    for (auto* s : edit->tempoSequence.getTimeSigs())
        meters.push_back({s->getStartBeat(), s->numerator.get(), s->denominator.get(), s->triplets.get()});
    auto sequence = [&] { return Sequence(tempos, meters, tracktion::tempo::LengthOfOneBeat::dependsOnTimeSignature); };
    auto seq = sequence();
    std::map<std::string, ClipModel> clips;
    std::map<std::string, std::string> tracks;
    std::set<std::string> refs;
    Json diff = Json::array();
    size_t operationIndex = 0;
    for (auto* t : te::getAudioTracks(*edit))
    {
        tracks[t->itemID.toString().toStdString()] = trackType(*t);
        for (auto* c : t->getClips())
            if (auto* m = dynamic_cast<te::MidiClip*>(c))
            {
                auto p = m->getPosition();
                auto& s = clips[m->itemID.toString().toStdString()];
                s = {m->getStartBeat().inBeats(),
                     m->getEndBeat().inBeats(),
                     m->getContentStartBeat().inBeats(),
                     samples(p.getStart()),
                     samples(p.getEnd()),
                     sourceSamples(p.getStartOfSource()),
                     m->getSyncType() == te::Clip::syncBarsBeats,
                     m->isLooping(),
                     {}};
                s.locked = bool(m->state.getProperty("ndaw_locked", false));
                s.playbackProcessed = !transformRestriction(*m).empty() && !m->isLooping();
                for (auto* n : m->getSequence().getNotes())
                    s.notes[id(n->state)] = {n->getStartBeat().inBeats(), n->getLengthBeats().inBeats(),
                                             n->getNoteNumber(), n->getVelocity(), n->isMute()};
            }
    }
    for (const auto& op : operations)
    {
        const std::string cmd = op["command"];
        const auto& a = op["args"];
        const auto index = operationIndex++;
        if (cmd == "track.create")
        {
            const std::string ref = a.at("ref");
            require(refs.insert(ref).second, "duplicate object reference");
            tracks[ref] = a.value("type", std::string("audio"));
        }
        else if (cmd == "plugin.insert" && a.at("type") == te::FourOscPlugin::xmlTypeName)
            tracks[a.at("track")] = "instrument";
        else if (cmd == "plugin.external.insert" && externalDescriptor(a.at("descriptor"))["instrument"])
            tracks[a.at("track")] = "instrument";
        else if (cmd.starts_with("tempo.event.") || cmd.starts_with("meter.event."))
        {
            const bool isTempo = cmd.starts_with("tempo."), removing = cmd.ends_with("delete"),
                       creating = cmd.ends_with("create");
            auto& ids = isTempo ? tempoIDs : meterIDs;
            const std::string key = creating ? "" : a.at("event").get<std::string>();
            require(creating || ids.contains(key), "musical event not found or already deleted");
            const auto previous = creating ? BP::fromBeats(-1) : ids.at(key);
            require(!removing || previous.inBeats() != 0, "initial musical event cannot be deleted");
            const auto beat = removing ? previous : BP::fromBeats(a.at("beat_position").get<double>());
            require(beat.inBeats() >= 0 && beat.inBeats() <= 1e8 &&
                        seq.toTime(beat).inSeconds() <= te::Edit::maximumLength,
                    "musical event position outside Edit range");
            require(creating || (previous.inBeats() == 0 ? beat.inBeats() == 0 : beat.inBeats() > 0),
                    "initial musical event must stay at zero; other events must stay after it");
            if (!isTempo && !removing)
            {
                auto boundary = seq;
                if (!creating && beat != previous && previous.inBeats() > 0)
                {
                    auto remaining = meters;
                    remaining.erase(std::remove_if(remaining.begin(), remaining.end(),
                                                   [&](const auto& t) { return t.startBeat == previous; }),
                                    remaining.end());
                    boundary = Sequence(tempos, remaining, tracktion::tempo::LengthOfOneBeat::dependsOnTimeSignature);
                }
                require(std::abs(boundary.toBarsAndBeats(boundary.toTime(beat)).beats.inBeats()) < 1e-5,
                        "meter event must start on a bar boundary without its previous setting");
            }
            if (isTempo)
            {
                auto found =
                    std::find_if(tempos.begin(), tempos.end(), [&](auto& t) { return t.startBeat == previous; });
                require(creating || found != tempos.end(), "ambiguous tempo event reference");
                require(removing || std::none_of(tempos.begin(), tempos.end(),
                                                 [&](auto& t)
                                                 {
                                                     return std::abs((t.startBeat - beat).inBeats()) < 1e-8 &&
                                                            (creating || t.startBeat != previous);
                                                 }),
                        "tempo event would overlap another event");
                if (removing)
                    tempos.erase(found);
                else
                {
                    const double bpm = a.at("bpm");
                    require(std::isfinite(bpm) && bpm >= 20 && bpm <= 300, "Tempo outside SDK 20..300 BPM");
                    if (creating)
                        tempos.push_back({beat, bpm, 1});
                    else
                    {
                        found->startBeat = beat;
                        found->bpm = bpm;
                    }
                }
                std::sort(tempos.begin(), tempos.end(), [](auto& x, auto& y) { return x.startBeat < y.startBeat; });
            }
            else
            {
                auto found =
                    std::find_if(meters.begin(), meters.end(), [&](auto& t) { return t.startBeat == previous; });
                require(creating || found != meters.end(), "ambiguous meter event reference");
                require(removing || std::none_of(meters.begin(), meters.end(),
                                                 [&](auto& t)
                                                 {
                                                     return std::abs((t.startBeat - beat).inBeats()) < 1e-8 &&
                                                            (creating || t.startBeat != previous);
                                                 }),
                        "meter event would overlap another event");
                if (removing)
                    meters.erase(found);
                else
                {
                    const int num = a.at("numerator"), den = a.at("denominator");
                    require(num >= 1 && num <= 32 &&
                                (den == 1 || den == 2 || den == 4 || den == 8 || den == 16 || den == 32),
                            "invalid meter");
                    if (creating)
                        meters.push_back({beat, num, den, false});
                    else
                    {
                        found->startBeat = beat;
                        found->numerator = num;
                        found->denominator = den;
                    }
                }
                std::sort(meters.begin(), meters.end(), [](auto& x, auto& y) { return x.startBeat < y.startBeat; });
            }
            if (!creating)
            {
                if (removing)
                    ids.erase(key);
                else
                    ids[key] = beat;
            }
            diff.push_back({{"command", cmd}, {"event", key}, {"before_beat", previous.inBeats()}, {"after", a}});
            seq = sequence();
            for (auto& [_, c] : clips)
                if (!c.beats)
                {
                    c.start = seq.toBeats(time(c.absoluteStart)).inBeats();
                    c.end = seq.toBeats(time(c.absoluteEnd)).inBeats();
                    c.content = seq.toBeats(time(c.absoluteContent)).inBeats();
                }
        }
        else if (cmd == "tempo.set" || cmd == "meter.set")
        {
            auto pos = a.at("position_samples").get<int64_t>();
            require(pos >= 0, "negative Tempo position");
            auto beat = seq.toBeats(time(pos));
            if (cmd == "tempo.set")
            {
                double bpm = a.at("bpm");
                require(std::isfinite(bpm) && bpm >= te::TempoSetting::minBPM && bpm <= te::TempoSetting::maxBPM,
                        "Tempo outside SDK 20..300 BPM");
                auto t = std::find_if(tempos.begin(), tempos.end(),
                                      [&](auto& t) { return std::abs((t.startBeat - beat).inBeats()) < 1e-8; });
                if (t != tempos.end())
                {
                    t->bpm = bpm;
                    t->curve = 1;
                }
                else
                    tempos.push_back({beat, bpm, 1});
                std::sort(tempos.begin(), tempos.end(), [](auto& a, auto& b) { return a.startBeat < b.startBeat; });
            }
            else
            {
                int num = a.at("numerator"), den = a.at("denominator");
                require(num >= 1 && num <= 32 &&
                            (den == 1 || den == 2 || den == 4 || den == 8 || den == 16 || den == 32),
                        "invalid meter");
                require(std::abs(seq.toBarsAndBeats(time(pos)).beats.inBeats()) < 1e-5,
                        "meter must start on a bar boundary");
                auto s = std::find_if(meters.begin(), meters.end(),
                                      [&](auto& s) { return std::abs((s.startBeat - beat).inBeats()) < 1e-8; });
                if (s != meters.end())
                {
                    s->numerator = num;
                    s->denominator = den;
                }
                else
                    meters.push_back({beat, num, den, false});
                std::sort(meters.begin(), meters.end(), [](auto& a, auto& b) { return a.startBeat < b.startBeat; });
            }
            seq = sequence();
            for (auto& [_, c] : clips)
                if (!c.beats)
                {
                    c.start = seq.toBeats(time(c.absoluteStart)).inBeats();
                    c.end = seq.toBeats(time(c.absoluteEnd)).inBeats();
                    c.content = seq.toBeats(time(c.absoluteContent)).inBeats();
                }
        }
        else if (cmd == "midi.clip.create")
        {
            const std::string track = a.at("track"), ref = a.at("ref");
            require(tracks.contains(track) && (tracks.at(track) == "midi" || tracks.at(track) == "instrument"),
                    "MIDI clip requires MIDI or instrument track");
            require(ref.starts_with("$") && ref.size() > 1 && refs.insert(ref).second,
                    "invalid or duplicate clip reference");
            require(!a.at("name").get<std::string>().empty(), "empty MIDI clip name");
            range(a);
            auto begin = a.at("position_samples").get<int64_t>(), end = begin + a.at("length_samples").get<int64_t>();
            double start = seq.toBeats(time(begin)).inBeats();
            clips[ref] = {start, seq.toBeats(time(end)).inBeats(), start, begin, end, begin, true, false, {}};
        }
        else if (cmd.starts_with("midi.clips.") || cmd.starts_with("timeline.clips."))
        {
            require(std::count_if(operations.begin(), operations.end(),
                                  [](const Json& o)
                                  {
                                      const auto id = o["command"].template get<std::string>();
                                      return id.starts_with("midi.clips.") || id.starts_with("timeline.clips.");
                                  }) == 1,
                    "one atomic MIDI clip operation per Plan");
            require(std::all_of(operations.begin(), operations.end(),
                                [&](const Json& o)
                                {
                                    return o == op || o["command"] == "session.range.set" ||
                                           o["command"] == "session.insertion.set" ||
                                           o["command"] == "session.range.clear";
                                }),
                    "MIDI clip clipboard cannot be combined with other engineering edits");
            require(!(a.value("ripple", false) || a.value("mode", std::string{}) == "shuffle") ||
                        a.contains("state_hash"),
                    "Shuffle requires a sealed native preview from makePlan");
            diff.push_back(midiClipClipboardChange(cmd, a, index));
        }
        else if (cmd == "midi.notes.erase" || cmd == "midi.notes.paste")
        {
            require(operations.size() == 1, "MIDI clipboard requires one atomic operation per Plan");
            diff.push_back(midiClipboardChange(cmd, a, index));
        }
        else if (cmd == "midi.notes.quantize" || cmd == "midi.notes.transpose" || cmd == "midi.notes.time")
        {
            const std::string target = a.at("clip");
            require(clips.contains(target), "MIDI clip not found");
            auto& c = clips.at(target);
            Json notes = Json::array();
            for (const auto& key : selectNotes(a, c, seq))
            {
                auto before = c.notes.at(key);
                auto after = cmd == "midi.notes.time" ? transformTimingNote(a, c, before, seq)
                                                      : transformNote(cmd, a, c, before);
                auto next = noteFacts(after, c, seq);
                require(next["length_samples"].get<int64_t>() > 0, "transformed note shorter than one session sample");
                notes.push_back({{"note", key}, {"before", noteFacts(before, c, seq)}, {"after", next}});
                c.notes[key] = after;
            }
            diff.push_back({{"command", cmd},
                            {"clip", target},
                            {"operation_index", index},
                            {"selection", a.at("selection")},
                            {"notes", notes},
                            {"grid_origin", "session beat zero"},
                            {"new_note_id_prefix", "#new-note- marks not-yet-created objects, not a native ID"}});
        }
        else if (cmd.starts_with("midi.note."))
        {
            const std::string target = a.at("clip");
            require(clips.contains(target), "MIDI clip not found");
            auto& c = clips.at(target);
            if (cmd != "midi.note.add")
            {
                const std::string note = a.at("note");
                require(c.notes.contains(note), "MIDI note not found or already removed");
                if (cmd == "midi.note.delete")
                {
                    c.notes.erase(note);
                    continue;
                }
            }
            range(a);
            int pitch = a.at("pitch"), velocity = a.at("velocity");
            require(pitch >= 0 && pitch <= 127 && velocity >= 1 && velocity <= 127, "invalid MIDI pitch or velocity");
            auto begin = a.at("position_samples").get<int64_t>(), end = begin + a.at("length_samples").get<int64_t>();
            auto startBeat = seq.toBeats(time(begin)).inBeats(), endBeat = seq.toBeats(time(end)).inBeats();
            require(startBeat >= c.start - 1e-5 && endBeat <= c.end + 1e-5 && startBeat >= c.content,
                    "note outside playable MIDI clip");
            std::string key =
                cmd == "midi.note.set" ? a.at("note").get<std::string>() : "#new-note-" + std::to_string(index);
            if (cmd == "midi.note.add" && a.contains("ref"))
            {
                key = a.at("ref");
                require(key.starts_with("$") && key.size() > 1 && refs.insert(key).second,
                        "invalid or duplicate note reference");
            }
            const bool muted = c.notes.contains(key) ? c.notes.at(key).muted : false;
            c.notes[key] = {startBeat - c.content, endBeat - startBeat, pitch, velocity, muted};
        }
    }
    return diff;
}
void Commands::executeMusicOperation(const std::string& cmd, const Json& input, Json& objects,
                                     std::map<std::string, std::string>& aliases)
{
    auto a = input;
    for (const char* key : {"clip", "note"})
        if (a.contains(key))
        {
            const std::string value = a.at(key);
            if (aliases.contains(value))
                a[key] = aliases.at(value);
        }
    auto& seq = edit->tempoSequence;
    auto& um = edit->getUndoManager();
    if (cmd.starts_with("midi.clips.") || cmd.starts_with("timeline.clips."))
    {
        executeMidiClipClipboard(cmd, a, objects);
        return;
    }
    if (cmd == "midi.notes.erase" || cmd == "midi.notes.paste")
    {
        executeMidiClipboard(cmd, a, objects);
        return;
    }
    if (cmd.starts_with("tempo.event.") || cmd.starts_with("meter.event."))
    {
        const bool isTempo = cmd.starts_with("tempo."), creating = cmd.ends_with("create"),
                   removing = cmd.ends_with("delete");
        te::EditTimecodeRemapperSnapshot snap;
        snap.savePreChangeState(*edit);
        juce::ValueTree state;
        const auto beat = tracktion::BeatPosition::fromBeats(a.value("beat_position", 0.));
        if (creating)
            state = isTempo ? seq.insertTempo(beat, a.at("bpm"), 1)->state : seq.insertTimeSig(beat)->state;
        else
        {
            const std::string key = a.at("event");
            if (isTempo)
                for (auto* t : seq.getTempos())
                    if (id(t->state) == key)
                    {
                        state = t->state;
                        break;
                    }
            if (!isTempo)
                for (auto* t : seq.getTimeSigs())
                    if (id(t->state) == key)
                    {
                        state = t->state;
                        break;
                    }
            require(state.isValid(), "musical event target disappeared");
        }
        auto parent = state.getParent();
        if (removing)
            parent.removeChild(state, &um);
        else
        {
            state.setProperty(te::IDs::startBeat, beat.inBeats(), &um);
            if (isTempo)
                state.setProperty(te::IDs::bpm, a.at("bpm").get<double>(), &um);
            else
            {
                state.setProperty(te::IDs::numerator, a.at("numerator").get<int>(), &um);
                state.setProperty(te::IDs::denominator, a.at("denominator").get<int>(), &um);
                if (creating)
                    state.setProperty(te::IDs::triplets, false, &um);
            }
            // Native meter insertion copies the prior state, including its ID. Allocate a fresh ID for every creation.
            if (creating)
                edit->createNewItemID().writeID(state, &um);
            struct Order
            {
                int compareElements(const juce::ValueTree& x, const juce::ValueTree& y) const
                {
                    const double a = x.getProperty(te::IDs::startBeat, 0.), b = y.getProperty(te::IDs::startBeat, 0.);
                    return a < b ? -1 : a > b ? 1 : 0;
                }
            } order;
            parent.sort(order, &um, true);
        }
        seq.updateTempoData();
        snap.remapEdit(*edit);
        objects.push_back({{"id", id(state)}, {"kind", isTempo ? "tempo" : "meter"}, {"deleted", removing}});
        return;
    }
    if (cmd == "tempo.set" || cmd == "meter.set")
    {
        te::EditTimecodeRemapperSnapshot snap;
        snap.savePreChangeState(*edit);
        auto beat = seq.toBeats(time(a.at("position_samples")));
        juce::ValueTree state;
        if (cmd == "tempo.set")
        {
            te::TempoSetting* existing = nullptr;
            for (auto* t : seq.getTempos())
                if (std::abs((t->getStartBeat() - beat).inBeats()) < 1e-8)
                    existing = t;
            if (existing)
            {
                existing->set(beat, a.at("bpm"), 1, false);
                state = existing->state;
            }
            else
                state = seq.insertTempo(beat, a.at("bpm"), 1)->state;
        }
        else
        {
            const int previousCount = seq.getNumTimeSigs();
            auto setting = seq.insertTimeSig(beat);
            if (seq.getNumTimeSigs() != previousCount)
            {
                edit->createNewItemID().writeID(setting->state, &um);
                setting->state.setProperty(te::IDs::triplets, false, &um);
            }
            setting->numerator = a.at("numerator").get<int>();
            setting->denominator = a.at("denominator").get<int>();
            state = setting->state;
        }
        if (te::EditItemID::fromID(state).isInvalid())
            edit->createNewItemID().writeID(state, &um);
        seq.updateTempoData();
        snap.remapEdit(*edit);
        objects.push_back({{"id", id(state)}, {"kind", cmd == "tempo.set" ? "tempo" : "meter"}});
        return;
    }
    if (cmd == "midi.clip.create")
    {
        auto* t = track(a.at("track"));
        require(t != nullptr, "MIDI target disappeared");
        auto start = time(a.at("position_samples"));
        auto end = time(a.at("position_samples").get<int64_t>() + a.at("length_samples").get<int64_t>());
        auto c = t->insertMIDIClip(juce::String(a.at("name").get<std::string>()), {start, end}, nullptr);
        require(c != nullptr, "MIDI clip creation failed");
        c->setSyncType(te::Clip::syncBarsBeats);
        aliases[a.at("ref")] = c->itemID.toString().toStdString();
        objects.push_back({{"id", c->itemID.toString().toStdString()}, {"kind", "midi_clip"}});
        return;
    }
    auto* c = midiClip(a.at("clip"));
    require(c != nullptr, "MIDI clip disappeared");
    te::MidiNote* note = nullptr;
    if (cmd == "midi.notes.quantize" || cmd == "midi.notes.transpose" || cmd == "midi.notes.time")
    {
        ClipModel model;
        model.start = c->getStartBeat().inBeats();
        model.end = c->getEndBeat().inBeats();
        model.content = c->getContentStartBeat().inBeats();
        model.looped = c->isLooping();
        model.locked = bool(c->state.getProperty("ndaw_locked", false));
        model.playbackProcessed = !transformRestriction(*c).empty() && !c->isLooping();
        std::map<std::string, te::MidiNote*> actual;
        for (auto* n : c->getSequence().getNotes())
        {
            auto key = id(n->state);
            model.notes[key] = {n->getStartBeat().inBeats(), n->getLengthBeats().inBeats(), n->getNoteNumber(),
                                n->getVelocity(), n->isMute()};
            actual[key] = n;
        }
        if (a.contains("note_ids"))
            for (auto& value : a["note_ids"])
            {
                const std::string key = value;
                if (aliases.contains(key))
                    value = aliases.at(key);
            }
        for (const auto& key : selectNotes(a, model, seq))
        {
            const auto before = model.notes.at(key), after = cmd == "midi.notes.time"
                                                                 ? transformTimingNote(a, model, before, seq)
                                                                 : transformNote(cmd, a, model, before);
            auto* n = actual.at(key);
            if (after.source != before.source || after.length != before.length)
                n->setStartAndLength(tracktion::BeatPosition::fromBeats(after.source),
                                     tracktion::BeatDuration::fromBeats(after.length), &um);
            if (after.pitch != before.pitch)
                n->setNoteNumber(after.pitch, &um);
            objects.push_back({{"id", key}, {"kind", "midi_note"}});
        }
        return;
    }
    if (cmd != "midi.note.add")
    {
        for (auto* n : c->getSequence().getNotes())
            if (id(n->state) == a.at("note").get<std::string>())
                note = n;
        require(note != nullptr, "MIDI note disappeared");
    }
    if (cmd == "midi.note.delete")
    {
        c->getSequence().removeNote(*note, &um);
        return;
    }
    auto begin = seq.toBeats(time(a.at("position_samples"))),
         end = seq.toBeats(time(a.at("position_samples").get<int64_t>() + a.at("length_samples").get<int64_t>()));
    auto relative = tracktion::toPosition(begin - c->getContentStartBeat());
    if (cmd == "midi.note.add")
    {
        note = c->getSequence().addNote(a.at("pitch"), relative, end - begin, a.at("velocity"), 0, &um);
        require(note != nullptr, "MIDI note insertion failed");
        edit->createNewItemID().writeID(note->state, &um);
        if (a.contains("ref"))
            aliases[a.at("ref")] = id(note->state);
    }
    else
    {
        note->setStartAndLength(relative, end - begin, &um);
        note->setNoteNumber(a.at("pitch"), &um);
        note->setVelocity(a.at("velocity"), &um);
    }
    objects.push_back({{"id", id(note->state)}, {"kind", "midi_note"}});
}
} // namespace ndaw::v2
