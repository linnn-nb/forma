// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "NativeMidiNumbers.h"
#include "NativeEditXml.h"
#include "HistoryDelta.h"
#include "ProjectDocument.h"
#include "OwnedEditFixture.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <limits>
using namespace ndaw::v2;
namespace ndaw::v2
{
class RecordingTestAccess
{
public:
    static juce::ValueTree state(Commands& c)
    {
        return c.edit->state.createCopy();
    }
    static Json cached(Commands& c)
    {
        Json rows = Json::array();
        for (auto* track : te::getAudioTracks(*c.edit))
            for (auto* clip : track->getClips())
                if (auto* midi = dynamic_cast<te::MidiClip*>(clip))
                    for (auto* note : midi->getSequence().getNotes())
                        rows.push_back({{"id", note->state[te::IDs::id].toString().toStdString()},
                                        {"b", std::bit_cast<uint64_t>(note->getStartBeat().inBeats())},
                                        {"l", std::bit_cast<uint64_t>(note->getLengthBeats().inBeats())}});
        for (auto* track : te::getAudioTracks(*c.edit))
            for (auto* clip : track->getClips())
                if (auto* midi = dynamic_cast<te::MidiClip*>(clip))
                    for (auto* cc : midi->getSequence().getControllerEvents())
                        rows.push_back({{"clip", midi->itemID.toString().toStdString()},
                                        {"type", cc->getType()},
                                        {"value", cc->getControllerValue()},
                                        {"b", std::bit_cast<uint64_t>(cc->getBeatPosition().inBeats())}});
        return rows;
    }
};
} // namespace ndaw::v2
namespace
{
Json result;
int checks = 0;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void rejects(F f, const char* why)
{
    std::string error;
    try
    {
        f();
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }
    result["rejections"].push_back(error);
    check(!error.empty(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File folder;
    explicit Storage(juce::File f) : PropertyStorage("Forma native MIDI numeric tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }
};
juce::String xml(juce::ValueTree tree)
{
    return preciseEditXml(tree)->toString();
}
juce::ValueTree parsed(juce::ValueTree tree)
{
    return juce::ValueTree::fromXml(*preciseEditXml(tree));
}
std::string digest(const juce::String& text)
{
    return juce::SHA256(text.toRawUTF8(), text.getNumBytesAsUTF8()).toHexString().toStdString();
}
Json bits(juce::ValueTree tree)
{
    Json rows = Json::array();
    // Read only the native clip structures independently of the decoder's visitor.
    auto clip = [&](juce::ValueTree c)
    {
        if (!c.hasType("MIDICLIP"))
            return;
        for (auto note : c.getChildWithName("SEQUENCE"))
            if (note.hasType("NOTE"))
            {
                check(note["b"].isDouble() && note["l"].isDouble(),
                      "native note values are typed doubles before SDK construction");
                rows.push_back({{"clip", c["id"].toString().toStdString()},
                                {"note", note["id"].toString().toStdString()},
                                {"b", std::bit_cast<uint64_t>(double(note["b"]))},
                                {"l", std::bit_cast<uint64_t>(double(note["l"]))}});
            }
            else if (note.hasType("CONTROL"))
            {
                check(note["b"].isDouble(), "native CC and pitch timestamps are typed doubles before SDK construction");
                rows.push_back({{"clip", c["id"].toString().toStdString()},
                                {"type", int(note["type"])},
                                {"value", int(note["val"])},
                                {"b", std::bit_cast<uint64_t>(double(note["b"]))}});
            }
    };
    for (auto track : tree)
        if (track.hasType("TRACK"))
        {
            for (auto c : track)
                clip(c);
            for (auto list : track.getChildWithName("NDAW_PLAYLISTS"))
                for (auto c : list)
                    clip(c);
        }
    return rows;
}
Json run(Commands& c, const char* command, Json args)
{
    return c.commit(c.makePlan("human", Json::array({{{"command", command}, {"args", args}}})));
}
void codec()
{
    const std::array<double, 16> values{0.00070833333333333317,
                                        0.0062500000000000056,
                                        0.1,
                                        0.012500000000000011,
                                        std::nextafter(.5, 0.),
                                        std::nextafter(.5, 1.),
                                        1.2345678901234567,
                                        13.333333333333334,
                                        1e-12,
                                        1e-300,
                                        std::numeric_limits<double>::min(),
                                        std::numeric_limits<double>::denorm_min(),
                                        std::numeric_limits<double>::max(),
                                        -0.0,
                                        0.0,
                                        1.0};
    juce::ValueTree original("EDIT"), track("TRACK"), clip("MIDICLIP"), sequence("SEQUENCE");
    track.setProperty("id", "10", nullptr);
    clip.setProperty("id", "11", nullptr);
    for (size_t i = 0; i < values.size(); ++i)
    {
        juce::ValueTree note("NOTE");
        note.setProperty("id", juce::String(i + 100), nullptr);
        note.setProperty("b", values[i], nullptr);
        note.setProperty("l", values[(i + 1) % values.size()], nullptr);
        sequence.addChild(note, -1, nullptr);
        juce::ValueTree control("CONTROL");
        control.setProperty("b", values[i], nullptr);
        control.setProperty("type", i % 2 ? 4101 : 1, nullptr);
        control.setProperty("val", int(i * 977) & 16383, nullptr);
        control.setProperty("l", "opaque-control-metadata", nullptr);
        sequence.addChild(control, -1, nullptr);
    }
    clip.addChild(sequence, -1, nullptr);
    track.addChild(clip, -1, nullptr);
    original.addChild(track, -1, nullptr);
    // These look like numeric MIDI but are opaque state, never playable notes.
    auto opaqueClip = clip.createCopy();
    opaqueClip.getChildWithName("SEQUENCE").getChild(0).setProperty("b", "not-a-number", nullptr);
    juce::ValueTree plugin("PLUGIN"), origin("NDAW_SAMPLE_MIDI_ORIGIN"), source("NDAW_MIDI_SOURCES");
    plugin.addChild(opaqueClip.createCopy(), -1, nullptr);
    origin.addChild(opaqueClip.createCopy(), -1, nullptr);
    source.addChild(opaqueClip.createCopy(), -1, nullptr);
    track.addChild(plugin, -1, nullptr);
    clip.addChild(origin, -1, nullptr);
    original.addChild(source, -1, nullptr);
    const auto expected = bits(original);
    auto loaded = parsed(original);
    Json rawDrift = Json::array();
    for (size_t i = 0; i < values.size(); ++i)
    {
        const auto raw = loaded.getChild(0).getChild(0).getChildWithName("SEQUENCE").getChild(int(2 * i + 1))["b"];
        const auto actual = std::bit_cast<uint64_t>(double(raw)), wanted = std::bit_cast<uint64_t>(values[i]);
        if (actual != wanted)
            rawDrift.push_back({{"text", raw.toString().toStdString()}, {"before", wanted}, {"unprepared", actual}});
    }
    result["unprepared_controller_drift"] = rawDrift;
    const auto opaqueBefore = xml(loaded.getChild(0).getChildWithName("PLUGIN"));
    native_midi_numbers::prepareEdit(loaded);
    check(bits(loaded) == expected,
          "native note/CC/pitch binary64 vectors including subnormal and signed zero have zero ULP drift");
    check(loaded.getChild(0).getChild(0).getChildWithName("SEQUENCE").getChild(1)["l"].isString(),
          "controller l is opaque metadata, not a note duration");
    check(xml(loaded.getChild(0).getChildWithName("PLUGIN")) == opaqueBefore &&
              xml(loaded.getChild(0).getChild(0).getChildWithName("NDAW_SAMPLE_MIDI_ORIGIN")) == xml(origin) &&
              xml(loaded.getChildWithName("NDAW_MIDI_SOURCES")) == xml(source),
          "opaque plugin, origin and source trees remain byte-for-byte unchanged");
    auto after = original.createCopy();
    auto seq = after.getChild(0).getChild(0).getChildWithName("SEQUENCE");
    seq.getChild(0).setProperty("b", values[2], nullptr);
    seq.getChild(0).setProperty("l", values[0], nullptr);
    seq.getChild(3).setProperty("b", values[6], nullptr);
    seq.removeChild(1, nullptr);
    auto added = sequence.getChild(2).createCopy();
    added.setProperty("id", "999", nullptr);
    seq.addChild(added, -1, nullptr);
    auto addedControl = sequence.getChild(1).createCopy();
    addedControl.setProperty("b", values[7], nullptr);
    seq.addChild(addedControl, -1, nullptr);
    auto delta = parsed(history_delta::difference(original, after));
    native_midi_numbers::prepareDelta(delta);
    history_delta::apply(loaded, delta, true);
    if (bits(loaded) != bits(after))
    {
        result["codec_forward_diff"] = Json::diff(bits(after), bits(loaded));
        result["codec_delta"] = xml(delta).toStdString();
    }
    check(bits(loaded) == bits(after),
          "typed property and structural history images restore exact double bits forward");
    history_delta::apply(loaded, delta, false);
    check(bits(loaded) == expected, "typed history images restore exact double bits backward");
    auto adjacent = original.createCopy();
    auto event = adjacent.getChild(0).getChild(0).getChildWithName("SEQUENCE").getChild(1);
    // Construct distinct native snapshots. JUCE's property setter may itself
    // coalesce adjacent doubles; this tests history persistence, not that setter.
    event.removeProperty("b", nullptr);
    event.setProperty("b", std::nextafter(values[0], 1.), nullptr);
    auto adjacentDelta = parsed(history_delta::difference(original, adjacent));
    check(adjacentDelta.getNumChildren() > 0,
          "one ULP native CC change is a real history delta even without reordering");
    native_midi_numbers::prepareDelta(adjacentDelta);
    history_delta::apply(loaded, adjacentDelta, true);
    check(bits(loaded) == bits(adjacent), "adjacent-double CC delta restores exact forward timestamp");
    history_delta::apply(loaded, adjacentDelta, false);
    check(bits(loaded) == expected, "adjacent-double CC Undo restores the exact original timestamp");
    for (const char* text : {"", "0.1junk", "1,25", "nan", "inf", "1e10000", "1e-10000"})
    {
        auto bad = parsed(original);
        bad.getChild(0).getChild(0).getChildWithName("SEQUENCE").getChild(0).setProperty("b", text, nullptr);
        rejects([&] { native_midi_numbers::prepareEdit(bad); },
                "malformed/nonfinite/out-of-range note number does not silently become zero");
        bad = parsed(original);
        bad.getChild(0).getChild(0).getChildWithName("SEQUENCE").getChild(1).setProperty("b", text, nullptr);
        rejects([&] { native_midi_numbers::prepareEdit(bad); },
                "malformed CC/pitch timestamps cannot silently become zero");
    }
    result["codec_vectors"] = expected;
}
juce::ValueTree bare(juce::ValueTree state)
{
    state = state.createCopy();
    auto info = state.getChildWithName("NATIVEDAW");
    info.removeChild(info.getChildWithName("PERSISTENT_HISTORY"), nullptr);
    info.removeChild(info.getChildWithName("UI"), nullptr);
    info.removeProperty("revision", nullptr);
    for (int i = info.getNumChildren(); --i >= 0;)
        if (info.getChild(i).hasType("REQUEST_AUDIT"))
            info.removeChild(i, nullptr);
    return state;
}
void write(juce::ValueTree state, const juce::File& file)
{
    auto doc = project_document::write(state, {});
    check(doc->writeTo(file), "actual owned project document writes");
}
juce::ValueTree load(const juce::File& file)
{
    auto doc = juce::parseXML(file);
    check(bool(doc), "actual project XML parses");
    return project_document::read(*doc).edit;
}
void compare(Commands& c, const Json& expected, const Json& cached)
{
    const auto actual = bits(RecordingTestAccess::state(c));
    if (actual != expected)
        result["bit_diff"] = Json::diff(expected, actual);
    check(actual == expected, "all active and inactive note properties preserve exact binary64 values");
    check(RecordingTestAccess::cached(c) == cached,
          "native MidiNote playback caches preserve the original exact binary64 values");
}
void rejectedOpen(Commands& c, juce::ValueTree state, const juce::File& file)
{
    write(state, file);
    const auto token = c.sessionToken();
    const auto old = bits(RecordingTestAccess::state(c));
    rejects([&] { c.open(file); }, "actual malformed project/history is refused before adoption");
    check(c.sessionToken() == token && bits(RecordingTestAccess::state(c)) == old,
          "rejected Open preserves current exact notes and session");
}
Json recoveryWait(Commands& c)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (c.recoveryStatus().value("busy", false) && juce::Time::getMillisecondCounterHiRes() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    auto status = c.recoveryStatus();
    check(!status.value("busy", false), "actual recovery I/O completes within fixed 5s");
    return status;
}
void project(const juce::File& root)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("prefs")));
    run(c, "track.create", {{"ref", "$m"}, {"name", "Precision Keys"}, {"type", "midi"}});
    const std::string track = c.query()["tracks"][0]["id"];
    const auto made = run(c, "midi.clip.create",
                          {{"track", track},
                           {"ref", "$clip"},
                           {"name", "Exact native notes"},
                           {"position_samples", 37},
                           {"length_samples", 24000}});
    const std::string clip = made["objects"][0]["id"];
    for (auto p : std::array<std::pair<int64_t, int64_t>, 4>{{{54, 256}, {5837, 150}, {3037, 300}, {5957, 256}}})
        run(c, "midi.note.add",
            {{"clip", clip},
             {"pitch", 60},
             {"velocity", 80},
             {"position_samples", p.first},
             {"length_samples", p.second}});
    Json cc = Json::array();
    for (double beat : {0.0062500000000000056, 0.012500000000000011, 0.1})
        cc.push_back({{"id", "cc:" + clip + ":new:" + juce::Uuid().toString().toStdString()},
                      {"source_beat", beat},
                      {"raw_value", (17 << 7) + int(cc.size())}});
    run(c, "midi.cc.replace",
        {{"clip", clip},
         {"controller", 1},
         {"state_hash", c.query()["tracks"][0]["clips"][0]["controller_state_hash"]},
         {"points", cc}});
    const std::string main = c.query()["tracks"][0]["playlists"]["active"];
    const auto copied = run(c, "playlist.create",
                            {{"track", track}, {"source", main}, {"name", "Inactive precise notes"}, {"role", "comp"}});
    const std::string alt = copied["objects"][0]["id"];
    const auto before = bits(RecordingTestAccess::state(c)), beforeCache = RecordingTestAccess::cached(c);
    const auto first = root.getChildFile("Before.tracktionedit");
    c.save(first);
    const auto firstState = load(first);
    const auto q = c.query();
    const std::string note = q["tracks"][0]["clips"][0]["notes"][0]["id"];
    const auto changed = run(c, "midi.note.set",
                             {{"clip", clip},
                              {"note", note},
                              {"pitch", 61},
                              {"velocity", 81},
                              {"position_samples", 55},
                              {"length_samples", 151}});
    const auto after = bits(RecordingTestAccess::state(c)), afterCache = RecordingTestAccess::cached(c);
    const auto second = root.getChildFile("After.tracktionedit");
    c.save(second);
    const auto secondState = load(second);
    const auto deleted = run(c, "midi.note.delete", {{"clip", clip}, {"note", note}});
    const auto removed = bits(RecordingTestAccess::state(c)), removedCache = RecordingTestAccess::cached(c);
    c.undo(deleted["plan_id"]);
    c.undo(changed["plan_id"]);
    const auto redoFile = root.getChildFile("Redo-Only.tracktionedit");
    c.save(redoFile);
    {
        Commands cold(false, std::make_unique<Storage>(root.getChildFile("cold")));
        cold.open(redoFile);
        compare(cold, before, beforeCache);
        cold.redo();
        compare(cold, after, afterCache);
        cold.redo();
        compare(cold, removed, removedCache);
        cold.undo(deleted["plan_id"]);
        compare(cold, after, afterCache);
        cold.undo(changed["plan_id"]);
        compare(cold, before, beforeCache);
        run(cold, "playlist.select", {{"track", track}, {"playlist", alt}});
        check(bits(RecordingTestAccess::state(cold)).size() == before.size(),
              "inactive sequence activates without losing any note");
        cold.undo();
        compare(cold, before, beforeCache);
        for (int round = 0; round < 3; ++round)
        {
            auto file = root.getChildFile("Round-" + juce::String(round) + ".tracktionedit");
            cold.save(file);
            cold.open(file);
            compare(cold, before, beforeCache);
        }
    }
    // Real schema1 snapshot images, with exact original hashes and genuine L1 transaction metadata.
    auto legacy = secondState.createCopy();
    auto info = legacy.getChildWithName("NATIVEDAW");
    info.removeChild(info.getChildWithName("PERSISTENT_HISTORY"), nullptr);
    juce::ValueTree archive("PERSISTENT_HISTORY");
    archive.setProperty("schema", 1, nullptr);
    archive.setProperty("cursor", 1, nullptr);
    const std::array<juce::ValueTree, 2> states{bare(firstState), bare(secondState)};
    for (int i = 0; i < 2; ++i)
    {
        juce::ValueTree step("STATE");
        const auto text = xml(states[i]);
        step.setProperty("xml", text, nullptr);
        step.setProperty("sha256", juce::String(digest(text)), nullptr);
        if (i)
            step.setProperty("plan_id", juce::String(changed["plan_id"].get<std::string>()), nullptr);
        archive.addChild(step, -1, nullptr);
    }
    archive.setProperty("current_hash", juce::String(digest(xml(states[1]))), nullptr);
    info.addChild(archive, -1, nullptr);
    const auto legacyFile = root.getChildFile("Legacy-History.tracktionedit");
    write(legacy, legacyFile);
    {
        Commands cold(false, std::make_unique<Storage>(root.getChildFile("legacy")));
        cold.open(legacyFile);
        compare(cold, after, afterCache);
        cold.undo(changed["plan_id"]);
        compare(cold, before, beforeCache);
        cold.redo();
        compare(cold, after, afterCache);
    }
    auto bad = secondState.createCopy();
    auto image = preciseEditXml(bad);
    discardDerivedFixtureHistory(*image);
    bad = juce::ValueTree::fromXml(*image);
    bad.getChildWithName("TRACK")
        .getChildWithName("MIDICLIP")
        .getChildWithName("SEQUENCE")
        .getChild(0)
        .setProperty("b", "0.1junk", nullptr);
    rejectedOpen(c, bad, root.getChildFile("Bad-Current.tracktionedit"));
    auto badPast = legacy.createCopy();
    auto history = badPast.getChildWithName("NATIVEDAW").getChildWithName("PERSISTENT_HISTORY");
    auto past = juce::ValueTree::fromXml(*juce::parseXML(history.getChild(0)["xml"].toString()));
    past.getChildWithName("TRACK")
        .getChildWithName("MIDICLIP")
        .getChildWithName("SEQUENCE")
        .getChild(0)
        .setProperty("l", "broken", nullptr);
    const auto pastText = xml(past);
    history.getChild(0).setProperty("xml", pastText, nullptr);
    history.getChild(0).setProperty("sha256", juce::String(digest(pastText)), nullptr);
    rejectedOpen(c, badPast, root.getChildFile("Bad-Past.tracktionedit"));
    const auto legacyHash = Commands::mediaHash(legacyFile);
    c.recoveryControl("session.recovery.start", Json::object());
    check(recoveryWait(c)["state"] == "idle", "actual local recovery service starts");
    c.recoveryControl("session.recovery.capture", Json::object());
    const auto snapshot = recoveryWait(c)["receipt"]["snapshot"];
    check(snapshot.contains("sha256"), "actual durable snapshot has a checksum receipt");
    run(c, "track.rename", {{"track", track}, {"name", "Changed after snapshot"}});
    c.recoveryControl("session.recovery.restore", {{"id", snapshot["id"]},
                                                   {"sha256", snapshot["sha256"]},
                                                   {"session_token", c.sessionToken()},
                                                   {"base_revision", c.querySummary()["revision"]}});
    check(recoveryWait(c)["state"] == "restored",
          "actual saved recovery snapshot is loaded through the normal L1 path");
    compare(c, before, beforeCache);
    check(Commands::mediaHash(legacyFile) == legacyHash,
          "original legacy fixture bytes are never overwritten by loading or migration");
    result["before_bits"] = before;
    result["after_bits"] = after;
    result["session"] = redoFile.getFullPathName().toStdString();
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("MIDI-Precision-" + juce::Uuid().toString());
    root.createDirectory();
    result = {{"root", root.getFullPathName().toStdString()},
              {"result", "running"},
              {"budgets", {{"ulp_error", 0}, {"reopen_rounds", 3}, {"recovery_io_ms", 5000}}}};
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2);
    int code = 0;
    try
    {
        codec();
        project(root);
        result["result"] = "passed";
    }
    catch (const std::exception& e)
    {
        code = 1;
        result["result"] = "failed";
        result["error"] = e.what();
        std::cerr << e.what() << std::endl;
    }
    result["checks"] = checks;
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2);
    std::cout << result.dump(2) << std::endl;
    return code;
}
