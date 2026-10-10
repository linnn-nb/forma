#include "SampleMidiMap.h"
#include "ui/Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Commands& owner(Workspace& w)
    {
        return w.commands;
    }
    static te::MidiClip& clip(Commands& c, const std::string& id)
    {
        return *c.midiClip(id);
    }
    static Json pending(Workspace& w)
    {
        return w.pending;
    }
    static void accept(Workspace& w, bool yes)
    {
        if (yes)
            w.acceptButton.onClick();
        else
            w.rejectButton.onClick();
    }
    static juce::String preview(Workspace& w)
    {
        return w.previewText.getText();
    }
    static void select(Workspace& w, const std::string& id)
    {
        w.select(id);
    }
    static juce::ValueTree state(Commands& c)
    {
        return c.edit->state.createCopy();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
void check(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
    ++checks;
    std::cout << "PASS " << message << '\n';
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(70);
}
template <class F> void rejects(F fn, const char* message)
{
    bool threw = false;
    try
    {
        fn();
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    check(threw, message);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File file) : te::PropertyStorage("Forma sample MIDI"), folder(file) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
Json op(const char* name, Json args)
{
    return {{"command", name}, {"args", std::move(args)}};
}
Json run(Commands& c, Json operations)
{
    auto plan = c.makePlan("human", std::move(operations));
    auto receipt = c.commit(plan);
    pump();
    check(receipt["state"] == "committed", "real native transaction committed");
    return receipt;
}
juce::XmlElement* byID(juce::XmlElement& tree, const std::string& id)
{
    if (tree.getStringAttribute("id").toStdString() == id)
        return &tree;
    for (auto* child = tree.getFirstChildElement(); child; child = child->getNextElement())
        if (auto* found = byID(*child, id))
            return found;
    return nullptr;
}
Json timing(Commands& c, const std::string& id)
{
    auto& clip = AudioDeviceTestAccess::clip(c, id);
    Json result = Json::array();
    for (auto* note : clip.getSequence().getNotes())
        result.push_back({{"kind", "NOTE"},
                          {"start", note->getEditStartTime(clip).inSeconds()},
                          {"end", note->getEditEndTime(clip).inSeconds()},
                          {"pitch", note->getNoteNumber()},
                          {"velocity", note->getVelocity()}});
    for (auto* event : clip.getSequence().getControllerEvents())
        result.push_back({{"kind", "CONTROL"},
                          {"start", event->getEditTime(clip).inSeconds()},
                          {"end", event->getEditTime(clip).inSeconds()},
                          {"type", event->getType()},
                          {"value", event->getControllerValue()},
                          {"metadata", event->getMetadata()}});
    for (auto* event : clip.getSequence().getSysexEvents())
        result.push_back({{"kind", "SYSEX"},
                          {"start", event->getEditTime(clip).inSeconds()},
                          {"end", event->getEditTime(clip).inSeconds()}});
    return result;
}
void sameTimes(Commands& c, const std::string& id, const Json& expected, double delta)
{
    const auto actual = timing(c, id);
    check(actual.size() == expected.size(), "all actual note CC SysEx events retained");
    for (size_t i = 0; i < expected.size(); ++i)
    {
        auto a = actual[i], e = expected[i];
        // 1 ns was declared before execution. Native beats/time double conversions,
        // not scheduling-block tolerance; integral sample projections also must agree.
        for (const char* field : {"start", "end"})
        {
            const double want = e[field].get<double>() + delta, got = a[field];
            check(std::abs(want - got) <= 1e-9, "native event absolute timestamp respects 1 ns budget");
            check(std::llround(want * 48000) == std::llround(got * 48000), "native event sample projection exact");
            a.erase(field);
            e.erase(field);
        }
        check(a == e, "native event type note velocity controller payload unchanged");
    }
}
std::string treeHash(const juce::ValueTree& tree)
{
    juce::MemoryOutputStream bytes;
    tree.writeToStream(bytes);
    return juce::SHA256(bytes.getData(), bytes.getDataSize()).toHexString().toStdString();
}
void origin(Commands& c, const std::string& id, const std::string& expected)
{
    const auto& state = AudioDeviceTestAccess::clip(c, id).state;
    sample_midi::validateOrigin(state);
    const auto provenance = state.getChildWithName("NDAW_SAMPLE_MIDI_ORIGIN");
    check(provenance.isValid() && provenance["source_hash"].toString().toStdString() == expected,
          "immutable original native sequence hash retained");
}
std::string inserted(const Json& receipt)
{
    for (const auto& x : receipt["objects"])
        if (x.contains("clipboard_token"))
            return x["id"];
    throw std::runtime_error("missing native inserted MIDI receipt");
}
Json playback(Commands& c, const std::string& id)
{
    auto& clip = AudioDeviceTestAccess::clip(c, id);
    const auto sequence = clip.getSequence().exportToPlaybackMidiSequence(clip, te::MidiList::TimeBase::seconds, false);
    Json result = Json::array();
    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        const auto& message = sequence.getEventPointer(i)->message;
        result.push_back(
            {{"payload", juce::String::toHexString(message.getRawData(), message.getRawDataSize()).toStdString()},
             {"seconds", message.getTimeStamp()}});
    }
    return result;
}
void samePlayback(Commands& c, const std::string& id, const Json& expected)
{
    const auto actual = playback(c, id);
    check(actual.size() == expected.size(), "native playback MIDI event count retained");
    for (size_t i = 0; i < actual.size(); ++i)
    {
        check(actual[i]["payload"] == expected[i]["payload"], "actual native playback MIDI bytes unchanged");
        check(std::abs(actual[i]["seconds"].get<double>() - expected[i]["seconds"].get<double>()) <= 1e-9,
              "actual native playback timestamps retain declared 1 ns budget");
    }
}
int renderOnset(Commands& c, const juce::File& file, int64_t start, int64_t end)
{
    auto receipt = c.render(file, start, end);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == end - start &&
              receipt["frames"] == end - start,
          "native FourOsc WAV independently decoded and length checked");
    juce::AudioBuffer<float> pcm(2, int(end - start));
    check(reader->read(&pcm, 0, pcm.getNumSamples(), 0, true, true), "actual native WAV samples read");
    for (int i = 0; i < pcm.getNumSamples(); ++i)
        if (std::abs(pcm.getSample(0, i)) > 3e-6)
            return i;
    throw std::runtime_error("actual instrument render silent");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto dir = juce::File(juce::String::fromUTF8(argc > 2 ? argv[2] : "/tmp/forma-sample-midi"))
                             .getChildFile("sample-midi-" + juce::Uuid().toString());
        dir.createDirectory();
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setSize(1440, 900);
        w.setVisible(true);
        pump();
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c, Json::array({op("tempo.event.create", {{"beat_position", 8.}, {"bpm", 80.}}),
                            op("tempo.event.create", {{"beat_position", 20.}, {"bpm", 160.}}),
                            op("meter.event.create", {{"beat_position", 24.}, {"numerator", 3}, {"denominator", 4}}),
                            op("meter.event.create", {{"beat_position", 36.}, {"numerator", 5}, {"denominator", 8}}),
                            op("track.create", {{"name", "Sample performance"}, {"type", "instrument"}, {"ref", "$s"}}),
                            op("track.create", {{"name", "Destination"}, {"type", "instrument"}, {"ref", "$d"}}),
                            op("midi.clip.create", {{"track", "$s"},
                                                    {"ref", "$c"},
                                                    {"name", "Original performance"},
                                                    {"position_samples", 96000},
                                                    {"length_samples", 240000}}),
                            op("midi.note.add", {{"clip", "$c"},
                                                 {"pitch", 64},
                                                 {"velocity", 75},
                                                 {"position_samples", 108000},
                                                 {"length_samples", 24000}}),
                            op("midi.note.add", {{"clip", "$c"},
                                                 {"pitch", 67},
                                                 {"velocity", 90},
                                                 {"position_samples", 168000},
                                                 {"length_samples", 64800}}),
                            op("midi.note.add", {{"clip", "$c"},
                                                 {"pitch", 69},
                                                 {"velocity", 60},
                                                 {"position_samples", 240000},
                                                 {"length_samples", 48000}}),
                            op("midi.clip.create", {{"track", "$s"},
                                                    {"ref", "$suffix"},
                                                    {"name", "Later sample performance"},
                                                    {"position_samples", 480000},
                                                    {"length_samples", 192000}}),
                            op("midi.note.add", {{"clip", "$suffix"},
                                                 {"pitch", 60},
                                                 {"velocity", 90},
                                                 {"position_samples", 492000},
                                                 {"length_samples", 72000}}),
                            op("midi.clip.create", {{"track", "$d"},
                                                    {"ref", "$dest"},
                                                    {"name", "Destination suffix"},
                                                    {"position_samples", 672000},
                                                    {"length_samples", 144000}}),
                            op("midi.note.add", {{"clip", "$dest"},
                                                 {"pitch", 60},
                                                 {"velocity", 70},
                                                 {"position_samples", 684000},
                                                 {"length_samples", 48000}})}));
        auto query = c.query();
        const std::string src = query["tracks"][0]["clips"][0]["id"], suffix = query["tracks"][0]["clips"][1]["id"],
                          dest = query["tracks"][1]["clips"][0]["id"], owner = query["tracks"][0]["id"],
                          target = query["tracks"][1]["id"];
        auto base = dir.getChildFile("Base.tracktionedit");
        c.save(base);
        auto xml = juce::XmlDocument::parse(base);
        auto* native = byID(*xml, src);
        native->setAttribute("offset", .5);
        auto* sequence = native->getChildByName("SEQUENCE");
        sequence->addChildElement(
            te::MidiControllerEvent::createControllerEvent(tracktion::BeatPosition::fromBeats(1.125), 1, 96 << 7, 7)
                .createXml()
                .release());
        const uint8_t payload[]{0x7d, 0x41};
        sequence->addChildElement(
            te::MidiSysexEvent::createSysexEvent(juce::MidiMessage::createSysExMessage(payload, 2),
                                                 tracktion::BeatPosition::fromBeats(3.25))
                .createXml()
                .release());
        for (auto* event : sequence->getChildIterator())
            event->setAttribute("opaque_sample_test", "retain exact text");
        const auto fixture = dir.getChildFile("Step.tracktionedit");
        check(xml->writeTo(fixture), "real native offset note CC SysEx fixture saved");
        auto rampXml = juce::XmlDocument::parse(fixture);
        for (auto* event : rampXml->getChildByName("TEMPOSEQUENCE")->getChildIterator())
            if (event->hasTagName("TEMPO") && event->getDoubleAttribute("startBeat") == 8)
                event->setAttribute("curve", .35);
        const auto ramp = dir.getChildFile("Ramp.tracktionedit");
        check(rampXml->writeTo(ramp), "real native ramp fixture saved");
        const auto inputHash = juce::SHA256(fixture).toHexString();
        auto open = [&](const juce::File& file)
        {
            w.openSession(file);
            pump();
        };
        auto samples = [&]
        {
            for (const auto& id : {src, suffix, dest})
                run(c, Json::array({op("midi.clip.timebase.set", {{"clip", id}, {"basis", "samples"}})}));
        };
        int cases = 0;
        for (const auto& file : {fixture, ramp})
        {
            open(file);
            samples();
            const auto initial = timing(c, src), tail = timing(c, suffix);
            const auto sourceHash = treeHash(AudioDeviceTestAccess::clip(c, src).getSequence().state);
            const auto initialPlayback = playback(c, src);
            const auto before = AudioDeviceTestAccess::state(c);
            auto change = run(c, Json::array({op("tempo.set", {{"position_samples", 0}, {"bpm", 95.}})}));
            sameTimes(c, src, initial, 0);
            sameTimes(c, suffix, tail, 0);
            origin(c, src, sourceHash);
            samePlayback(c, src, initialPlayback);
            c.undo(change["plan_id"]);
            pump();
            sameTimes(c, src, initial, 0);
            check(!AudioDeviceTestAccess::clip(c, src).state.getChildWithName("NDAW_SAMPLE_MIDI_ORIGIN").isValid(),
                  "single native Undo removes projection provenance and restores event tree");
            c.redo();
            pump();
            sameTimes(c, src, initial, 0);
            const auto saved = dir.getChildFile("Tempo-" + juce::String(cases) + ".tracktionedit");
            c.save(saved);
            open(saved);
            sameTimes(c, src, initial, 0);
            origin(c, src, sourceHash);
            samePlayback(c, src, initialPlayback);
            run(c, Json::array({op("meter.set", {{"position_samples", 0}, {"numerator", 4}, {"denominator", 8}})}));
            sameTimes(c, src, initial, 0);
            origin(c, src, sourceHash);
            samePlayback(c, src, initialPlayback);
            ++cases;
            for (bool range : {false, true})
            {
                open(file);
                samples();
                const auto expected = timing(c, src), suffixExpected = timing(c, dest);
                const auto frozenHash = treeHash(AudioDeviceTestAccess::clip(c, src).getSequence().state);
                auto buffer = range ? c.prepareTimelineRangeClipboard(Json::array({owner}), 120000, 192000,
                                                                      c.sessionToken(), c.querySummary()["revision"])
                                    : c.prepareTimelineClipClipboard(Json::array({src}), c.sessionToken(),
                                                                     c.querySummary()["revision"]);
                c.acceptClipboard(buffer["id"]);
                const double sourceStart = buffer["start_samples"].get<int64_t>() / 48000., delta = 9 - sourceStart;
                const auto envelope = c.timelineClipPasteRange(buffer["id"], 432000);
                const double suffixDelta = (envelope["end_samples"].get<int64_t>() - 432000) / 48000.;
                auto plan =
                    c.makePlan("human", Json::array({op("timeline.clips.paste", {{"clipboard", buffer["id"]},
                                                                                 {"tracks", Json::array({target})},
                                                                                 {"position_samples", 432000},
                                                                                 {"mode", "shuffle"},
                                                                                 {"ripple_mapping", "samples"}})}));
                check(
                    c.preview(plan)["timeline_changes"][0]["clips"].back()["after"].contains("sample_midi_projection"),
                    "native preview exposes real sample event projection");
                auto receipt = c.commit(plan);
                pump();
                const auto pasted = inserted(receipt);
                sameTimes(c, pasted, expected, delta);
                sameTimes(c, dest, suffixExpected, suffixDelta);
                origin(c, pasted, frozenHash);
                const auto savedPaste = dir.getChildFile("Paste-" + juce::String(cases) + ".tracktionedit");
                c.save(savedPaste);
                c.undo(receipt["plan_id"]);
                pump();
                sameTimes(c, dest, suffixExpected, 0);
                c.redo();
                pump();
                sameTimes(c, pasted, expected, delta);
                open(savedPaste);
                sameTimes(c, pasted, expected, delta);
                origin(c, pasted, frozenHash);
                ++cases;
            }
            for (bool objects : {false, true})
            {
                open(file);
                samples();
                const auto expected = timing(c, suffix);
                auto buffer = objects ? c.prepareTimelineClipClipboard(Json::array({src}), c.sessionToken(),
                                                                       c.querySummary()["revision"])
                                      : c.prepareTimelineRangeClipboard(Json::array({owner}), 144000, 192000,
                                                                        c.sessionToken(), c.querySummary()["revision"]);
                c.acceptClipboard(buffer["id"]);
                const double delta = objects ? -5. : -1.;
                auto receipt = run(c, Json::array({op("timeline.clips.erase", {{"clipboard", buffer["id"]},
                                                                               {"action", "delete"},
                                                                               {"ripple", true},
                                                                               {"ripple_mapping", "samples"}})}));
                sameTimes(c, suffix, expected, delta);
                c.undo(receipt["plan_id"]);
                pump();
                sameTimes(c, suffix, expected, 0);
                c.redo();
                pump();
                sameTimes(c, suffix, expected, delta);
                const auto savedClear = dir.getChildFile("Clear-" + juce::String(cases) + ".tracktionedit");
                c.save(savedClear);
                open(savedClear);
                sameTimes(c, suffix, expected, delta);
                ++cases;
            }
        }
        open(ramp);
        samples();
        run(c, Json::array({op("tempo.set", {{"position_samples", 0}, {"bpm", 95.}})}));
        const auto archivedHash = AudioDeviceTestAccess::clip(c, src)
                                      .state.getChildWithName("NDAW_SAMPLE_MIDI_ORIGIN")["source_hash"]
                                      .toString()
                                      .toStdString();
        const auto noteID = c.query()["tracks"][0]["clips"][0]["notes"][0]["id"];
        run(c, Json::array({op("midi.note.set", {{"clip", src},
                                                 {"note", noteID},
                                                 {"pitch", 62},
                                                 {"velocity", 81},
                                                 {"position_samples", 132000},
                                                 {"length_samples", 36000}})}));
        const auto editedTimes = timing(c, src), editedPlayback = playback(c, src);
        auto events = c.query()["music"]["tempos"];
        auto eventReceipt = run(
            c, Json::array({op("tempo.event.set", {{"event", events[1]["id"]}, {"beat_position", 7.}, {"bpm", 67.}})}));
        sameTimes(c, src, editedTimes, 0);
        samePlayback(c, src, editedPlayback);
        origin(c, src, archivedHash);
        c.undo(eventReceipt["plan_id"]);
        pump();
        sameTimes(c, src, editedTimes, 0);
        c.redo();
        pump();
        sameTimes(c, src, editedTimes, 0);
        run(c, Json::array({op("tempo.event.delete", {{"event", events[1]["id"]}})}));
        sameTimes(c, src, editedTimes, 0);
        samePlayback(c, src, editedPlayback);
        run(c, Json::array({op("tempo.event.create", {{"beat_position", 9.}, {"bpm", 73.}})}));
        sameTimes(c, src, editedTimes, 0);
        samePlayback(c, src, editedPlayback);
        auto remappedBuffer =
            c.prepareTimelineClipClipboard(Json::array({src}), c.sessionToken(), c.querySummary()["revision"]);
        c.acceptClipboard(remappedBuffer["id"]);
        run(c, Json::array({op("tempo.set", {{"position_samples", 0}, {"bpm", 105.}})}));
        auto remappedPaste = run(c, Json::array({op("timeline.clips.paste", {{"clipboard", remappedBuffer["id"]},
                                                                             {"tracks", Json::array({target})},
                                                                             {"position_samples", 432000},
                                                                             {"mode", "replace"}})}));
        sameTimes(c, inserted(remappedPaste), editedTimes, 7.);
        origin(c, inserted(remappedPaste), archivedHash);
        const auto projectedFile = dir.getChildFile("Projected.tracktionedit");
        c.save(projectedFile);
        open(projectedFile);
        const auto unchanged = c.query();
        for (bool corruptTimes : {false, true})
        {
            auto bad = juce::XmlDocument::parse(projectedFile);
            auto* provenance = byID(*bad, src)->getChildByName("NDAW_SAMPLE_MIDI_ORIGIN");
            check(provenance != nullptr, "saved native project contains original MIDI provenance");
            provenance->setAttribute(corruptTimes ? "source_times" : "source_hash",
                                     corruptTimes ? "[{\"index\":0}]" : "bad checksum");
            const auto badFile = dir.getChildFile(corruptTimes ? "BadTimes.tracktionedit" : "BadHash.tracktionedit");
            check(bad->writeTo(badFile), "owned damaged project fixture saved");
            rejects([&] { c.open(badFile); }, "damaged provenance rejected before native Edit adoption");
            check(c.query() == unchanged, "failed project open preserves live native project and revision");
        }
        for (bool nested : {false, true})
        {
            auto bad = juce::XmlDocument::parse(fixture);
            auto* clip = byID(*bad, src);
            if (nested)
                clip->getChildByName("SEQUENCE")
                    ->getChildByName("NOTE")
                    ->addChildElement(new juce::XmlElement("EXPRESSION"));
            else
                clip->setAttribute("loopLengthBeats", 2.);
            const auto badFile = dir.getChildFile(nested ? "Nested.tracktionedit" : "Loop.tracktionedit");
            check(bad->writeTo(badFile), "owned unsupported MIDI fixture saved");
            open(badFile);
            const auto beforeReject = c.query();
            rejects(
                [&]
                {
                    c.makePlan("human",
                               Json::array({op("midi.clip.timebase.set", {{"clip", src}, {"basis", "samples"}})}));
                },
                "unsupported loop or expression mapping rejected before writes");
            check(c.query() == beforeReject, "unsupported MIDI timebase rejection is atomic");
        }
        open(fixture);
        auto basisPlan =
            c.makePlan("human", Json::array({op("midi.clip.timebase.set", {{"clip", src}, {"basis", "samples"}})}));
        Scope basisScope;
        basisScope.mode = Permission::Preview;
        basisScope.targets = {src};
        basisScope.commands = {"midi.clip.timebase.set"};
        basisScope.end = 48000;
        rejects([&] { c.review(basisPlan, basisScope); }, "bounded time grant cannot change whole-performance clock");
        basisScope.end = std::numeric_limits<int64_t>::max();
        check(!c.review(basisPlan, basisScope)["permission"]["automatic_allowed"].get<bool>(),
              "timebase change requires explicit local confirmation under scoped API");
        AudioDeviceTestAccess::select(w, owner);
        pump();
        c.updateUiState({{"object_selection", Json::array({{{"id", src}, {"track", owner}, {"kind", "clip"}}})}},
                        c.sessionToken());
        pump();
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress sampleKey(juce::KeyPress::F8Key,
                                       juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier, 0);
        check(keys->containsMapping(editCommand::midiBasisSamples, sampleKey),
              "sample timebase default shortcut registered");
        check(w.keyPressed(sampleKey), "production GUI sample timebase shortcut dispatched");
        pump();
        check(AudioDeviceTestAccess::clip(c, src).getSyncType() == te::Clip::syncAbsolute,
              "GUI changes real native timebase");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(AudioDeviceTestAccess::clip(c, src).getSyncType() == te::Clip::syncBarsBeats, "GUI timebase single Undo");
        w.keyPressed(sampleKey);
        pump();
        const juce::KeyPress custom(
            'k', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        keys->clearAllKeyPresses(editCommand::midiBasisSamples);
        keys->addKeyPress(editCommand::midiBasisSamples, custom);
        pump();
        const auto demo = dir.getChildFile("SampleMidiReady.tracktionedit");
        c.save(demo);
        open(demo);
        check(keys->containsMapping(editCommand::midiBasisSamples, custom),
              "custom MIDI timebase shortcut persists through Open");
        rejects(
            [&]
            {
                c.makePlan("agent:test",
                           Json::array({op("midi.clip.timebase.set", {{"clip", src}, {"basis", "beats"}})}));
            },
            "external Agent cannot bypass local MIDI timebase permission");
        auto stale =
            c.makePlan("human", Json::array({op("midi.clip.timebase.set", {{"clip", src}, {"basis", "beats"}})}));
        run(c, Json::array({op("track.gain", {{"track", owner}, {"db", -6.}})}));
        rejects([&] { c.commit(stale); }, "human edit rejects stale timebase Plan");
        auto first = renderOnset(c, dir.getChildFile("BeforeTempo.wav"), 0, 8 * 48000);
        run(c, Json::array({op("tempo.set", {{"position_samples", 0}, {"bpm", 75.}})}));
        auto second = renderOnset(c, dir.getChildFile("AfterTempo.wav"), 0, 8 * 48000);
        check(first == second, "actual native instrument onset sample unchanged by Tempo edit");
        check(juce::SHA256(fixture).toHexString() == inputHash, "original native input bytes unchanged");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"mapping_cases", cases},
                    {"native_event_time_budget_seconds", 1e-9},
                    {"demo", demo.getFullPathName().toStdString()},
                    {"render_onset_sample", first},
                    {"limits", "nonlooped MIDI without native quantisation/groove or nested expression; original "
                               "sequence provenance retained; Mac physical GUI/listening unexecuted"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
