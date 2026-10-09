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
    static float value(Commands& c, const std::string& track, double seconds)
    {
        te::AutomationIterator iterator(*c.automationParameter(track, "volume"));
        iterator.setPosition(tracktion::TimePosition::fromSeconds(seconds));
        return iterator.getCurrentValue();
    }
    static Json pending(Workspace& w)
    {
        return w.pending;
    }
    static void confirm(Workspace& w, bool yes)
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
};
} // namespace ndaw::v2
namespace
{
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
    bool bad = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        bad = true;
    }
    check(bad, why);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(90);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma mixed Shuffle qualification"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
Json run(Commands& c, Json ops)
{
    auto receipt = c.commit(c.makePlan("human", std::move(ops)));
    check(receipt["state"] == "committed", "real native L1 transaction committed");
    pump();
    return receipt;
}
juce::XmlElement* byID(juce::XmlElement& x, const std::string& id)
{
    if (x.getStringAttribute("id").toStdString() == id)
        return &x;
    for (auto* child = x.getFirstChildElement(); child; child = child->getNextElement())
        if (auto* found = byID(*child, id))
            return found;
    return nullptr;
}
Json nativeData(const juce::File& saved, const std::string& id)
{
    auto xml = juce::XmlDocument::parse(saved);
    auto* clip = byID(*xml, id);
    check(clip != nullptr, "saved native clip identity exists");
    Json events = Json::array();
    auto visit = [&](auto&& self, juce::XmlElement& x) -> void
    {
        if (x.hasTagName("NOTE") || x.hasTagName("CONTROL") || x.hasTagName("SYSEX"))
        {
            Json event{{"kind", x.getTagName().toStdString()}};
            for (int i = 0; i < x.getNumAttributes(); ++i)
                if (x.getAttributeName(i) != "id")
                    event[x.getAttributeName(i).toStdString()] = x.getAttributeValue(i).toStdString();
            events.push_back(std::move(event));
        }
        for (auto* child = x.getFirstChildElement(); child; child = child->getNextElement())
            self(self, *child);
    };
    visit(visit, *clip);
    return {{"events", events}, {"opaque", clip->getStringAttribute("qualification_extra").toStdString()}};
}
Json clip(Commands& c, const std::string& id)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        for (const auto& v : t["clips"])
            if (v["id"] == id)
                return v;
    throw std::runtime_error("clip missing");
}
Json persistentTracks(Commands& c)
{
    auto tracks = c.query()["tracks"];
    for (auto& t : tracks)
    {
        // gain_db is a live native DSP readout; the versioned engineering
        // state is the explicit base plus the complete real automation lanes.
        t["gain_db"] = t["base_gain_db"];
        auto lanes = c.automationQuery(t["id"])["lanes"];
        for (auto& lane : lanes)
            for (const char* key : {"value", "display", "recording"})
                lane.erase(key);
        t["verified_automation"] = std::move(lanes);
    }
    return tracks;
}
Json pasted(const Json& receipt)
{
    Json result = Json::array();
    for (const auto& o : receipt["objects"])
        if (o.contains("clipboard_token"))
            result.push_back(o["id"]);
    return result;
}
void key(Workspace& w, char ch, bool alt = false)
{
    check(w.keyPressed(juce::KeyPress(
              ch, juce::ModifierKeys::commandModifier | (alt ? juce::ModifierKeys::altModifier : 0), ch)),
          "production timeline shortcut dispatched");
    pump();
}
Json curve(const std::string& track, int64_t at, double value, double shape = 0.)
{
    return operation("automation.point.add", {{"track", track},
                                              {"parameter", "volume"},
                                              {"position_samples", at},
                                              {"value", value},
                                              {"curve", shape},
                                              {"ref", "$p" + std::to_string(at)}});
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& path, int64_t start, int64_t end)
{
    auto r = c.render(path, start, end);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(path));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == end - start &&
              r["frames"] == end - start,
          "actual rendered WAV independently checked for format channels and duration");
    juce::AudioBuffer<float> pcm(2, int(end - start));
    check(reader->read(&pcm, 0, pcm.getNumSamples(), 0, true, true), "actual rendered PCM independently decoded");
    return pcm;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto dir = parent.getChildFile("shuffle-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1440, 900);
        auto& c = AudioDeviceTestAccess::owner(w);
        const auto media = dir.getChildFile("Stereo44100.wav");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = wav.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
        check(bool(writer), "real 44.1k stereo source writer created");
        juce::AudioBuffer<float> signal(2, 4 * 44100);
        for (int i = 0; i < signal.getNumSamples(); ++i)
        {
            signal.setSample(0, i, .1f * std::sin(float(i) * .047f));
            signal.setSample(1, i, .065f * std::cos(float(i) * .073f));
        }
        check(writer->writeFromAudioSampleBuffer(signal, 0, signal.getNumSamples()), "known real PCM written");
        writer.reset();
        const auto mediaHash = Commands::mediaHash(media);
        Json ops = Json::array({operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}})});
        for (int i = 0; i < 8; ++i)
        {
            const std::string ref = "$t" + std::to_string(i);
            const bool audio = i % 4 == 0 || i % 4 == 2;
            ops.push_back(
                operation("track.create", {{"name", (i < 4 ? "Source " : "Destination ") + std::to_string(i % 4)},
                                           {"type", audio        ? "audio"
                                                    : i % 4 == 1 ? "instrument"
                                                                 : "midi"},
                                           {"ref", ref}}));
            ops.push_back(operation("track.gain", {{"track", ref}, {"db", -18.}}));
            if (i == 0 || i == 4)
                ops.push_back(operation("clip.import", {{"track", ref},
                                                        {"path", media.getFullPathName().toStdString()},
                                                        {"media_hash", mediaHash},
                                                        {"position_samples", (i < 4 ? 1 : 8) * 48000}}));
            else if (i == 1 || i == 5)
            {
                const std::string cr = "$c" + std::to_string(i);
                ops.push_back(operation("midi.clip.create", {{"track", ref},
                                                             {"ref", cr},
                                                             {"name", "Native performance"},
                                                             {"position_samples", (i < 4 ? 1 : 8) * 48000},
                                                             {"length_samples", (i < 4 ? 4 : 6) * 48000}}));
                ops.push_back(operation("midi.note.add", {{"clip", cr},
                                                          {"pitch", 67},
                                                          {"velocity", 78},
                                                          {"position_samples", (i < 4 ? 1 : 8) * 48000},
                                                          {"length_samples", (i < 4 ? 3 : 5) * 48000}}));
            }
        }
        run(c, ops);
        auto q = c.query();
        Json source = Json::array(), target = Json::array();
        for (int i = 0; i < 4; ++i)
        {
            source.push_back(q["tracks"][i]["id"]);
            target.push_back(q["tracks"][i + 4]["id"]);
        }
        const std::string srcAudio = q["tracks"][0]["clips"][0]["id"], srcMidi = q["tracks"][1]["clips"][0]["id"];
        const std::string destMidi = q["tracks"][5]["clips"][0]["id"];
        const auto lateMedia = dir.getChildFile("LateStereo44100.wav");
        check(media.copyFileTo(lateMedia), "independent real suffix media copied for stale-media qualification");
        const auto late =
            run(c, Json::array({operation("clip.import", {{"track", target[0]},
                                                          {"path", lateMedia.getFullPathName().toStdString()},
                                                          {"media_hash", mediaHash},
                                                          {"position_samples", 16 * 48000}}),
                                operation("midi.clip.create", {{"track", target[1]},
                                                               {"ref", "$late"},
                                                               {"name", "Later native MIDI"},
                                                               {"position_samples", 16 * 48000},
                                                               {"length_samples", 2 * 48000}}),
                                operation("midi.note.add", {{"clip", "$late"},
                                                            {"pitch", 60},
                                                            {"velocity", 80},
                                                            {"position_samples", 16 * 48000},
                                                            {"length_samples", 48000}})}));
        q = c.query();
        const std::string lateAudio = q["tracks"][4]["clips"].back()["id"],
                          lateMidi = q["tracks"][5]["clips"].back()["id"];
        for (const auto& id : source)
            run(c, Json::array({curve(id, 0, -24.), curve(id, 2 * 48000, -18., .75), curve(id, 3 * 48000, -6.),
                                curve(id, 24 * 48000, -12.)}));
        for (const auto& id : target)
            run(c, Json::array({curve(id, 0, -30.), curve(id, 7 * 48000, -18., -.75), curve(id, 15 * 48000, -6., .75),
                                curve(id, 24 * 48000, -12.)}));
        const auto initial = dir.getChildFile("Base.tracktionedit");
        c.save(initial);
        auto xml = juce::XmlDocument::parse(initial);
        for (const auto& id : {srcMidi, destMidi, lateMidi})
        {
            auto* native = byID(*xml, id);
            native->setAttribute("qualification_extra", "native-events-survive-Shuffle");
            auto* seq = native->getChildByName("SEQUENCE");
            seq->addChildElement(
                te::MidiControllerEvent::createControllerEvent(tracktion::BeatPosition::fromBeats(.5), 1, 96 << 7, 0)
                    .createXml()
                    .release());
            const uint8_t payload[] = {0x7d, 0x12};
            seq->addChildElement(te::MidiSysexEvent::createSysexEvent(juce::MidiMessage::createSysExMessage(payload, 2),
                                                                      tracktion::BeatPosition::fromBeats(.75))
                                     .createXml()
                                     .release());
        }
        const auto fixture = dir.getChildFile("Fixture.tracktionedit");
        check(xml->writeTo(fixture), "owned native event fixture written");
        const auto fixtureHash = juce::SHA256(fixture).toHexString();
        auto open = [&]
        {
            w.openSession(fixture);
            pump();
        };
        auto copy = [&]
        {
            auto b = c.prepareTimelineRangeClipboard(source, 2 * 48000, 3 * 48000, c.sessionToken(),
                                                     c.querySummary()["revision"]);
            c.acceptClipboard(b["id"]);
            return b;
        };
        double maximumCurveError = 0, maximumPcmError = 0;
        size_t probes = 0;
        Json cases = Json::array();
        for (const int64_t removed : {int64_t{0}, int64_t{24000}, int64_t{96000}, int64_t{192000}})
        {
            open();
            const auto before = persistentTracks(c), b = copy();
            const int64_t point = 9 * 48000, end = point + 96000, delta = 96000 - removed;
            const Json args{{"clipboard", b["id"]},
                            {"tracks", target},
                            {"position_samples", point},
                            {"removal_end_samples", point + removed},
                            {"mode", "shuffle"}};
            auto plan = c.makePlan("human", Json::array({operation("timeline.clips.paste", args)}));
            auto preview = c.preview(plan);
            const auto impact = preview["timeline_changes"][0];
            check(impact["ripple"] == true && impact["displacement_samples"] == delta &&
                      impact["range"]["end_samples"] == end && impact["automation"].size() == 4 &&
                      plan["operations"][0]["args"].contains("state_hash"),
                  "sealed preview covers common clocks replacement delta and both empty-track curves");
            Scope limited;
            limited.targets = target.get<std::set<std::string>>();
            c.review(plan, limited);
            limited.targets.erase(target.back().get<std::string>());
            rejects([&] { c.review(plan, limited); }, "silent target remains inside required permission scope");
            std::vector<std::vector<float>> expected(4);
            for (int t = 0; t < 4; ++t)
                for (int i = 0; i < 512; ++i)
                    expected[t].push_back(AudioDeviceTestAccess::value(c, target[t], 16. + i / 128.));
            const auto receipt = c.commit(plan);
            pump();
            check(c.commit(plan)["replayed"] == true && pasted(receipt).size() == 2,
                  "native Shuffle inserts exactly once on retry");
            check(clip(c, lateAudio)["start_samples"] == 16 * 48000 + delta &&
                      clip(c, lateMidi)["start_samples"] == 16 * 48000 + delta &&
                      clip(c, lateMidi)["length_samples"] == 2 * 48000,
                  "later audio and actual MIDI shift by common samples retaining native MIDI duration");
            bool budget = true;
            for (int t = 0; t < 4; ++t)
                for (int i = 0; i < 512; ++i)
                {
                    const auto value = expected[t][i];
                    const double error = std::abs(
                        double(AudioDeviceTestAccess::value(c, target[t], 16. + i / 128. + delta / 48000.) - value));
                    const double ulp =
                        std::max(std::nextafter(value, INFINITY) - value, value - std::nextafter(value, -INFINITY));
                    maximumCurveError = std::max(maximumCurveError, error);
                    budget &= error <= 1e-7 + 2 * ulp;
                    ++probes;
                }
            check(budget, "actual suffix automation iterator follows common ripple within fixed numeric budget");
            const auto saved = dir.getChildFile("ShufflePaste" + juce::String(removed) + ".tracktionedit");
            c.save(saved);
            const auto after = persistentTracks(c);
            check(nativeData(saved, lateMidi) == nativeData(fixture, lateMidi),
                  "native suffix raw notes CC SysEx and opaque fields retained");
            for (const auto& id : pasted(receipt))
                if (clip(c, id)["kind"] == "midi")
                    check(nativeData(saved, id) == nativeData(fixture, srcMidi),
                          "inserted MIDI retains complete source events");
            c.undo(receipt["plan_id"]);
            pump();
            check(persistentTracks(c) == before, "one native Undo after Save restores clips curves and explicit bases");
            c.redo();
            pump();
            check(persistentTracks(c) == after, "one native Redo restores stable suffix and inserted IDs");
            w.openSession(saved);
            pump();
            check(persistentTracks(c) == after, "Shuffle native state survives Save and Open");
            run(c, Json::array({operation("track.solo", {{"track", target[0]}, {"enabled", true}})}));
            auto actual = render(c, dir.getChildFile("Actual" + juce::String(removed) + ".wav"), 16 * 48000 + delta,
                                 20 * 48000 + delta);
            open();
            run(c, Json::array({operation("track.solo", {{"track", target[0]}, {"enabled", true}})}));
            auto reference =
                render(c, dir.getChildFile("Reference" + juce::String(removed) + ".wav"), 16 * 48000, 20 * 48000);
            double error = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 2048; i < reference.getNumSamples() - 2048; ++i)
                    error = std::max(error, std::abs(double(actual.getSample(ch, i) - reference.getSample(ch, i))));
            maximumPcmError = std::max(maximumPcmError, error);
            check(reference.getMagnitude(0, reference.getNumSamples()) > .001 && error <= 2e-5,
                  "real 44.1k media suffix renders at shifted range within predeclared PCM tolerance");
            if (removed == 0)
            {
                w.openSession(saved);
                pump();
                run(c, Json::array({operation("track.solo", {{"track", target[1]}, {"enabled", true}})}));
                auto midiPcm =
                    render(c, dir.getChildFile("ShiftedNativeMidi.wav"), 16 * 48000 + delta, 20 * 48000 + delta);
                check(midiPcm.getMagnitude(0, midiPcm.getNumSamples()) > .001,
                      "actual moved MIDI notes drive native instrument and independently decoded WAV");
            }
            cases.push_back({{"removed_samples", removed}, {"displacement_samples", delta}, {"pcm_error", error}});
        }
        open();
        auto b = copy();
        auto plan = c.makePlan("human", Json::array({operation("timeline.clips.paste", {{"clipboard", b["id"]},
                                                                                        {"tracks", target},
                                                                                        {"position_samples", 9 * 48000},
                                                                                        {"mode", "shuffle"}})}));
        juce::MemoryBlock originalBytes;
        check(lateMedia.loadFileAsData(originalBytes), "owned media fault backup captured");
        const uint8_t byte = 1;
        {
            juce::FileOutputStream out(lateMedia);
            check(out.openedOk(), "owned fault stream opened");
            out.setPosition(100);
            out.write(&byte, 1);
            out.flush();
        }
        rejects([&] { c.commit(plan); },
                "changed independent suffix media refuses sealed Shuffle without partial edits");
        check(lateMedia.replaceWithData(originalBytes.getData(), originalBytes.getSize()),
              "owned media fault restored");
        auto stale = plan;
        run(c, Json::array({operation("track.rename", {{"track", target[0]}, {"name", "Later human edit"}})}));
        const auto human = persistentTracks(c);
        rejects([&] { c.commit(stale); }, "stale Shuffle cannot overwrite later human edit");
        check(persistentTracks(c) == human, "version conflict leaves later human state intact");
        open();
        b = copy();
        rejects(
            [&]
            {
                c.makePlan("human", Json::array({operation("timeline.clips.paste", {{"clipboard", b["id"]},
                                                                                    {"tracks", source},
                                                                                    {"position_samples", 2 * 48000},
                                                                                    {"mode", "shuffle"}})}));
            },
            "suffix crossing actual Tempo boundary refuses before writing native events");
        open();
        b = copy();
        run(c,
            Json::array({operation("clip.lock", {{"clip", lateAudio}, {"media_hash", mediaHash}, {"locked", true}})}));
        const auto lockedState = persistentTracks(c);
        rejects(
            [&]
            {
                c.makePlan("human", Json::array({operation("timeline.clips.paste", {{"clipboard", b["id"]},
                                                                                    {"tracks", target},
                                                                                    {"position_samples", 9 * 48000},
                                                                                    {"mode", "shuffle"}})}));
            },
            "locked later audio refuses entire mixed Shuffle including MIDI");
        check(persistentTracks(c) == lockedState, "locked suffix refusal makes no partial project edit");
        open();
        b = copy();
        const auto rangeBuffer = c.prepareTimelineRangeClipboard(target, 9 * 48000, 10 * 48000, c.sessionToken(),
                                                                 c.querySummary()["revision"]);
        const auto cutBefore = persistentTracks(c);
        plan = c.makePlan("human", Json::array({operation("timeline.clips.erase",
                                                          {{"clipboard", rangeBuffer["id"]}, {"ripple", true}})}));
        const auto cut = c.commit(plan);
        pump();
        check(clip(c, lateAudio)["start_samples"] == 15 * 48000 && clip(c, lateMidi)["start_samples"] == 15 * 48000,
              "range Shuffle Cut collapses complete sample interval for both media types");
        const auto cutFile = dir.getChildFile("ShuffleCut.tracktionedit");
        c.save(cutFile);
        const auto cutState = persistentTracks(c);
        c.undo(cut["plan_id"]);
        pump();
        check(persistentTracks(c) == cutBefore, "Shuffle Cut remains one Undo after Save");
        c.redo();
        pump();
        check(persistentTracks(c) == cutState, "Shuffle Cut Redo retains split native states");
        w.openSession(cutFile);
        pump();
        check(persistentTracks(c) == cutState, "Shuffle Cut persists across Open");
        open();
        b = copy();
        w.uiCommands().invokeDirectly(editCommand::shuffle, false);
        pump();
        run(c, Json::array({operation("session.range.clear", Json::object()),
                            operation("session.insertion.set", {{"position_samples", 9 * 48000}})}));
        AudioDeviceTestAccess::select(w, target[0]);
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", target}}, c.sessionToken());
        pump();
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom(
            'j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        keys->clearAllKeyPresses(editCommand::paste);
        keys->addKeyPress(editCommand::paste, custom);
        pump();
        const auto guiBefore = persistentTracks(c);
        check(w.keyPressed(custom), "custom Paste key dispatches production Shuffle command");
        pump();
        check(!AudioDeviceTestAccess::pending(w).is_null() && persistentTracks(c) == guiBefore &&
                  AudioDeviceTestAccess::preview(w).contains(juce::String::fromUTF8("96000")),
              "all mixed Shuffle edits show actual displacement preview before committing");
        AudioDeviceTestAccess::confirm(w, false);
        pump();
        check(persistentTracks(c) == guiBefore && c.clipboard() == b,
              "rejecting Shuffle preview preserves project and clipboard");
        check(w.keyPressed(custom), "custom key recreates actual Shuffle preview");
        pump();
        AudioDeviceTestAccess::confirm(w, true);
        pump();
        check(clip(c, lateMidi)["start_samples"] == 18 * 48000, "GUI acceptance executes actual native Shuffle");
        const auto guiSave = dir.getChildFile("ShuffleAccepted.tracktionedit");
        c.save(guiSave);
        const auto guiState = persistentTracks(c);
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(persistentTracks(c) == guiBefore, "GUI Shuffle acceptance is one actual Undo");
        w.openSession(guiSave);
        pump();
        check(persistentTracks(c) == guiState && keys->containsMapping(editCommand::paste, custom),
              "GUI Shuffle state and custom key survive Open");
        open();
        b = copy();
        w.uiCommands().invokeDirectly(editCommand::shuffle, false);
        pump();
        run(c,
            Json::array({operation("session.range.set", {{"start_samples", 9 * 48000}, {"end_samples", 10 * 48000}})}));
        AudioDeviceTestAccess::select(w, target[0]);
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", target}}, c.sessionToken());
        pump();
        const auto deleteBefore = persistentTracks(c);
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey)),
              "default Delete shortcut reaches mixed range Shuffle");
        pump();
        check(!AudioDeviceTestAccess::pending(w).is_null() &&
                  AudioDeviceTestAccess::pending(w)["operations"][0]["command"] == "timeline.clips.erase",
              "mixed range Delete produces actual native mixed-media confirmation card");
        AudioDeviceTestAccess::confirm(w, true);
        pump();
        check(clip(c, lateMidi)["start_samples"] == 15 * 48000 && c.clipboard() == b,
              "GUI Delete shifts MIDI and preserves previous clipboard");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(persistentTracks(c) == deleteBefore, "GUI range Delete restores whole native edit in one Undo");
        open();
        b = copy();
        w.uiCommands().invokeDirectly(editCommand::shuffle, false);
        pump();
        run(c,
            Json::array({operation("session.range.set", {{"start_samples", 9 * 48000}, {"end_samples", 10 * 48000}})}));
        AudioDeviceTestAccess::select(w, target[0]);
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", target}}, c.sessionToken());
        pump();
        const auto guiCutBefore = persistentTracks(c);
        key(w, 'x');
        check(!AudioDeviceTestAccess::pending(w).is_null() &&
                  AudioDeviceTestAccess::pending(w)["operations"][0]["command"] == "timeline.clips.erase",
              "CmdX previews real mixed range Shuffle Cut");
        AudioDeviceTestAccess::confirm(w, true);
        pump();
        check(clip(c, lateMidi)["start_samples"] == 15 * 48000 && c.clipboard()["start_samples"] == 9 * 48000 &&
                  c.clipboard()["tracks"] == target && c.query().at("time_selection").is_null(),
              "CmdX collapses mixed interval clears range and accepts actual new clipboard");
        const auto guiCutFile = dir.getChildFile("GuiShuffleCut.tracktionedit");
        c.save(guiCutFile);
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(persistentTracks(c) == guiCutBefore, "GUI mixed Cut is one native Undo after Save");
        open();
        w.uiCommands().invokeDirectly(editCommand::shuffle, false);
        pump();
        keys->clearAllKeyPresses(editCommand::paste);
        keys->addKeyPress(editCommand::paste, custom);
        pump();
        run(c,
            Json::array({operation("session.range.set", {{"start_samples", 2 * 48000}, {"end_samples", 3 * 48000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", source}}, c.sessionToken());
        pump();
        const auto demo = dir.getChildFile("MixedShuffleReady.tracktionedit");
        c.save(demo);
        check(Commands::mediaHash(media) == mediaHash && Commands::mediaHash(lateMedia) == mediaHash &&
                  juce::SHA256(fixture).toHexString() == fixtureHash,
              "all actual native edits preserve source media and fixture bytes");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"cases", cases},
                    {"native_iterator_probes", probes},
                    {"maximum_curve_error", maximumCurveError},
                    {"maximum_pcm_error", maximumPcmError},
                    {"demo", demo.getFullPathName().toStdString()},
                    {"limits", "native range Shuffle only; MIDI suffix must retain one constant Tempo/Meter corridor; "
                               "object Shuffle, partial loops, Warp and physical listening unexecuted"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
