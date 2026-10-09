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
    static const tracktion::tempo::Sequence& sequence(Commands& c)
    {
        c.edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(0));
        return c.edit->tempoSequence.getInternalSequence();
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
    explicit Storage(juce::File f) : PropertyStorage("Forma musical Shuffle qualification"), folder(f) {}
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
namespace
{
double maximumSavedBeatRounding = 0;
bool sameSavedState(const Json& a, const Json& b, const std::string& path = "")
{
    if (a == b)
        return true;
    const auto key = path.substr(path.find_last_of('/') + 1);
    const bool derived = key == "start_beat" || key == "content_start_beat" || key == "beat" ||
                         (key == "length_beats" && path.find("/notes/") == std::string::npos);
    if (a.is_number() && b.is_number() && derived)
    {
        const double error = std::abs(a.get<double>() - b.get<double>());
        maximumSavedBeatRounding = std::max(maximumSavedBeatRounding, error);
        // Same 1e-10 native-beat geometry tolerance used above for musical length.
        // Source event fields, IDs, sample positions, explicit bases and curve points stay exact.
        return error <= 1e-10;
    }
    if (a.type() != b.type() || a.size() != b.size())
        return false;
    if (a.is_object())
    {
        for (auto i = a.begin(); i != a.end(); ++i)
            if (!b.contains(i.key()) || !sameSavedState(i.value(), b.at(i.key()), path + "/" + i.key()))
                return false;
        return true;
    }
    if (a.is_array())
    {
        for (size_t i = 0; i < a.size(); ++i)
            if (!sameSavedState(a[i], b[i], path + "/" + std::to_string(i)))
                return false;
        return true;
    }
    return false;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto dir = parent.getChildFile("musical-shuffle-" + juce::Uuid().toString());
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
        Json ops = Json::array(
            {operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}}),
             operation("tempo.event.create", {{"beat_position", 16.}, {"bpm", 180.}}),
             operation("tempo.event.create", {{"beat_position", 32.}, {"bpm", 80.}}),
             operation("meter.event.create", {{"beat_position", 20.}, {"numerator", 3}, {"denominator", 8}}),
             operation("meter.event.create", {{"beat_position", 32.}, {"numerator", 4}, {"denominator", 4}})});
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
        auto rampXml = juce::XmlDocument::parse(fixture);
        auto* tempo = rampXml->getChildByName("TEMPOSEQUENCE");
        check(tempo != nullptr, "real native Tempo sequence found in owned fixture");
        int changedRamps = 0;
        for (auto* event = tempo->getFirstChildElement(); event; event = event->getNextElement())
            if (event->hasTagName("TEMPO"))
            {
                const double beat = event->getDoubleAttribute("startBeat");
                if (beat == 8.)
                {
                    event->setAttribute("curve", .35);
                    ++changedRamps;
                }
                if (beat == 16.)
                {
                    event->setAttribute("curve", -.25);
                    ++changedRamps;
                }
            }
        check(changedRamps == 2, "two actual native Tempo ramp coefficients installed in owned fixture");
        const auto ramp = dir.getChildFile("RampFixture.tracktionedit");
        check(rampXml->writeTo(ramp), "owned native ramp fixture saved");
        juce::File active = fixture;
        auto open = [&]
        {
            w.openSession(active);
            pump();
        };
        auto copy = [&]
        {
            auto b = c.prepareTimelineRangeClipboard(source, 2 * 48000, 3 * 48000, c.sessionToken(),
                                                     c.querySummary()["revision"]);
            c.acceptClipboard(b["id"]);
            return b;
        };

        auto beats = [&](double seconds)
        { return AudioDeviceTestAccess::sequence(c).toBeats(tracktion::TimePosition::fromSeconds(seconds)).inBeats(); };
        auto seconds = [&](double beat)
        { return AudioDeviceTestAccess::sequence(c).toTime(tracktion::BeatPosition::fromBeats(beat)).inSeconds(); };
        auto mapped = [&](double at, double delta) { return seconds(beats(at) + delta); };
        double maximumError = 0., maximumPcm = 0.;
        size_t probes = 0;
        Json cases = Json::array();
        open();
        check(c.shuffleOptions() == Json({{"schema", 1}, {"mapping", "samples"}}),
              "old projects retain qualified sample Shuffle default");
        check(c.query().at("shuffle_options") == c.querySummary().at("shuffle_options"),
              "full and summary expose same actual mapping");
        auto mappingPlan =
            c.makePlan("human", Json::array({operation("session.shuffle.mapping.set", {{"mapping", "native"}})}));
        auto mappingReceipt = c.commit(mappingPlan);
        pump();
        const auto mappingFile = dir.getChildFile("Mapping.tracktionedit");
        c.save(mappingFile);
        c.undo(mappingReceipt["plan_id"]);
        pump();
        check(c.shuffleOptions()["mapping"] == "samples", "mapping selection Undo restores missing legacy state");
        c.redo();
        pump();
        check(c.shuffleOptions()["mapping"] == "native", "mapping selection Redo restores native state");
        w.openSession(mappingFile);
        pump();
        check(c.shuffleOptions()["mapping"] == "native", "actual mapping survives Save Open");
        rejects(
            [&]
            {
                c.makePlan("agent:test",
                           Json::array({operation("session.shuffle.mapping.set", {{"mapping", "samples"}})}));
            },
            "Agent cannot widen frozen local editing settings");
        for (const auto& file : {fixture, ramp})
        {
            active = file;
            for (const int64_t removed : {int64_t{0}, int64_t{24000}, int64_t{96000}, int64_t{192000}})
            {
                open();
                const auto before = persistentTracks(c), b = copy();
                const int64_t point = 9 * 48000;
                const int64_t end = c.timelineClipPasteRange(b["id"], point)["end_samples"];
                const int64_t removalEnd = point + removed, delta = end - removalEnd;
                const double deltaBeat = beats(end / 48000.) - beats(removalEnd / 48000.);
                const auto oldMidi = clip(c, lateMidi);
                const int64_t expectedStart =
                    std::llround(mapped(oldMidi["start_samples"].get<int64_t>() / 48000., deltaBeat) * 48000.);
                Json args{{"clipboard", b["id"]},      {"tracks", target},
                          {"position_samples", point}, {"removal_end_samples", removalEnd},
                          {"mode", "shuffle"},         {"ripple_mapping", "native"}};
                auto sampleArgs = args;
                sampleArgs["ripple_mapping"] = "samples";
                if (delta != 0)
                    rejects([&] { c.makePlan("human", Json::array({operation("timeline.clips.paste", sampleArgs)})); },
                            "sample mode refuses displaced MIDI performance crossing real Tempo or Meter");
                else
                    check(!c.makePlan("human", Json::array({operation("timeline.clips.paste", sampleArgs)})).is_null(),
                          "equal replacement needs no suffix movement and remains valid in sample mode");
                auto plan = c.makePlan("human", Json::array({operation("timeline.clips.paste", args)}));
                const auto preview = c.preview(plan)["timeline_changes"][0];
                check(preview["ripple_mapping"] == "native" &&
                          std::abs(preview["displacement_beats"].get<double>() - deltaBeat) < 1e-12 &&
                          preview["destination_tempo_hash"].is_string(),
                      "native preview binds actual Tempo map and common musical displacement");
                Scope limited;
                limited.targets = target.get<std::set<std::string>>();
                c.review(plan, limited);
                limited.targets.erase(target.back().get<std::string>());
                rejects([&] { c.review(plan, limited); }, "native musical Scope includes empty target curve");
                std::vector<std::vector<std::pair<double, float>>> expected(4);
                for (int track = 0; track < 4; ++track)
                    for (int i = 0; i < 1024; ++i)
                    {
                        const int64_t at = end + 1 + i * 401;
                        const double sourceAt =
                            track % 2 == 0 ? (at - delta) / 48000. : mapped(at / 48000., -deltaBeat);
                        expected[track].push_back(
                            {at / 48000., AudioDeviceTestAccess::value(c, target[track], sourceAt)});
                    }
                const auto receipt = c.commit(plan);
                pump();
                check(c.commit(plan)["replayed"] == true, "musical Shuffle retry inserts once");
                check(clip(c, lateAudio)["start_samples"] == 16 * 48000 + delta &&
                          clip(c, lateMidi)["start_samples"] == expectedStart,
                      "audio retains sample displacement and MIDI uses actual musical displacement");
                const auto moved = clip(c, lateMidi);
                check(std::abs(moved["length_beats"].get<double>() - oldMidi["length_beats"].get<double>()) < 1e-10 &&
                          moved["notes"][0]["id"] == oldMidi["notes"][0]["id"] &&
                          moved["notes"][0]["source_beat"] == oldMidi["notes"][0]["source_beat"] &&
                          moved["notes"][0]["length_beats"] == oldMidi["notes"][0]["length_beats"],
                      "musical MIDI preserves native note IDs source beats and durations");
                bool within = true;
                for (int track = 0; track < 4; ++track)
                    for (auto [at, value] : expected[track])
                    {
                        const double error =
                            std::abs(double(AudioDeviceTestAccess::value(c, target[track], at) - value));
                        const double ulp =
                            std::max(std::nextafter(value, INFINITY) - value, value - std::nextafter(value, -INFINITY));
                        maximumError = std::max(maximumError, error);
                        within &= error <= 1e-7 + 2 * ulp;
                        ++probes;
                    }
                if (!within)
                    std::cout << "CURVE_ERROR " << maximumError << std::endl;
                check(within, "actual suffix DSP values follow each native clock within unchanged numeric budget");
                const auto saved = dir.getChildFile(file.getFileNameWithoutExtension() + "-Paste" +
                                                    juce::String(removed) + ".tracktionedit");
                c.save(saved);
                const auto after = persistentTracks(c);
                check(nativeData(saved, lateMidi) == nativeData(file, lateMidi),
                      "saved moved MIDI retains raw CC SysEx opaque attributes");
                c.undo(receipt["plan_id"]);
                pump();
                check(persistentTracks(c) == before,
                      "one native musical Undo after Save restores complete track state");
                c.redo();
                pump();
                check(persistentTracks(c) == after, "native musical Redo restores exact IDs and curves");
                w.openSession(saved);
                pump();
                if (!sameSavedState(persistentTracks(c), after))
                    std::cout << "OPEN_DIFF " << Json::diff(after, persistentTracks(c)).dump() << std::endl;
                check(sameSavedState(persistentTracks(c), after),
                      "musical suffix raw state survives Open with bounded derived-beat recomputation");
                const auto resaved = dir.getChildFile("RoundTrip-" + saved.getFileName());
                c.save(resaved);
                for (const auto& t : c.query()["tracks"])
                    for (const auto& v : t["clips"])
                        if (v["kind"] == "midi")
                            check(nativeData(saved, v["id"]) == nativeData(resaved, v["id"]),
                                  "round-trip raw MIDI event and opaque storage fields remain exact");
                if (removed == 0)
                {
                    run(c, Json::array({operation("track.solo", {{"track", target[0]}, {"enabled", true}})}));
                    auto actual = render(c, dir.getChildFile(file.getFileNameWithoutExtension() + "-MovedAudio.wav"),
                                         16 * 48000 + delta, 20 * 48000 + delta);
                    open();
                    run(c, Json::array({operation("track.solo", {{"track", target[0]}, {"enabled", true}})}));
                    auto reference =
                        render(c, dir.getChildFile(file.getFileNameWithoutExtension() + "-OriginalAudio.wav"),
                               16 * 48000, 20 * 48000);
                    double error = 0;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 2048; i < reference.getNumSamples() - 2048; ++i)
                            error =
                                std::max(error, std::abs(double(actual.getSample(ch, i) - reference.getSample(ch, i))));
                    maximumPcm = std::max(maximumPcm, error);
                    check(reference.getMagnitude(0, reference.getNumSamples()) > .001 && error <= 2e-5,
                          "sample-based suffix PCM is independently verified under native mixed Shuffle");
                    w.openSession(saved);
                    pump();
                    run(c, Json::array({operation("track.solo", {{"track", target[1]}, {"enabled", true}})}));
                    auto actualMidi = render(c, dir.getChildFile(file.getFileNameWithoutExtension() + "-MovedMidi.wav"),
                                             expectedStart, expectedStart + 4 * 48000);
                    check(actualMidi.getMagnitude(0, actualMidi.getNumSamples()) > .001,
                          "actual shifted MIDI drives native instrument and decoded WAV");
                }
                cases.push_back({{"profile", file.getFileName().toStdString()},
                                 {"removed_samples", removed},
                                 {"sample_delta", delta},
                                 {"beat_delta", deltaBeat},
                                 {"midi_start_samples", expectedStart}});
            }
            for (auto [low, high] : {std::pair<int64_t, int64_t>{9 * 48000, 10 * 48000}, {0, 48000}})
            {
                open();
                const auto owners = low == 0 ? source : target;
                const auto before = persistentTracks(c);
                auto b =
                    c.prepareTimelineRangeClipboard(owners, low, high, c.sessionToken(), c.querySummary()["revision"]);
                const double deltaBeat = beats(low / 48000.) - beats(high / 48000.);
                std::vector<std::vector<std::pair<double, float>>> expected(4);
                for (int track = 0; track < 4; ++track)
                    for (int i = 0; i < 512; ++i)
                    {
                        const int64_t at = low + 1 + i * 701;
                        const double src =
                            track % 2 == 0 ? (at + high - low) / 48000. : mapped(at / 48000., -deltaBeat);
                        expected[track].push_back({at / 48000., AudioDeviceTestAccess::value(c, owners[track], src)});
                    }
                auto plan = c.makePlan(
                    "human",
                    Json::array({operation("timeline.clips.erase",
                                           {{"clipboard", b["id"]}, {"ripple", true}, {"ripple_mapping", "native"}})}));
                auto receipt = c.commit(plan);
                pump();
                bool within = true;
                for (int track = 0; track < 4; ++track)
                    for (auto [at, value] : expected[track])
                    {
                        const double error =
                            std::abs(double(AudioDeviceTestAccess::value(c, owners[track], at) - value));
                        const double ulp =
                            std::max(std::nextafter(value, INFINITY) - value, value - std::nextafter(value, -INFINITY));
                        maximumError = std::max(maximumError, error);
                        within &= error <= 1e-7 + 2 * ulp;
                        ++probes;
                    }
                if (!within)
                    std::cout << "CUT_CURVE_ERROR " << maximumError << std::endl;
                check(within, "native Cut suffix follows musical map including session-zero guard");
                const auto saved = dir.getChildFile(file.getFileNameWithoutExtension() + "-Cut" + juce::String(low) +
                                                    ".tracktionedit");
                c.save(saved);
                const auto after = persistentTracks(c);
                c.undo(receipt["plan_id"]);
                pump();
                check(persistentTracks(c) == before, "native Cut one Undo restores actual whole selection");
                c.redo();
                pump();
                check(persistentTracks(c) == after, "native Cut Redo restores native curves and media geometry");
                w.openSession(saved);
                pump();
                check(sameSavedState(persistentTracks(c), after),
                      "native Cut Save Open matches raw state with bounded derived-beat recomputation");
            }
        }
        active = fixture;
        open();
        auto b = copy();
        auto plan =
            c.makePlan("human", Json::array({operation("timeline.clips.paste", {{"clipboard", b["id"]},
                                                                                {"tracks", target},
                                                                                {"position_samples", 9 * 48000},
                                                                                {"mode", "shuffle"},
                                                                                {"ripple_mapping", "native"}})}));
        const auto stale = plan;
        run(c, Json::array({operation("tempo.event.create", {{"beat_position", 40.}, {"bpm", 100.}})}));
        const auto human = persistentTracks(c);
        rejects([&] { c.commit(stale); }, "Tempo change invalidates musical Plan before commit");
        check(persistentTracks(c) == human, "Tempo conflict preserves later human edit");
        open();
        b = copy();
        run(c, Json::array({operation("clip.import", {{"track", target[1]},
                                                      {"path", media.getFullPathName().toStdString()},
                                                      {"media_hash", mediaHash},
                                                      {"position_samples", 16 * 48000}})}));
        const auto mixedBefore = persistentTracks(c);
        rejects(
            [&]
            {
                c.makePlan("human", Json::array({operation("timeline.clips.paste", {{"clipboard", b["id"]},
                                                                                    {"tracks", target},
                                                                                    {"position_samples", 9 * 48000},
                                                                                    {"mode", "shuffle"},
                                                                                    {"ripple_mapping", "native"}})}));
            },
            "same destination track with mixed suffix clocks and shared curves refuses atomic operation");
        check(persistentTracks(c) == mixedBefore, "ambiguous clock refusal makes no partial edit");
        open();
        b = copy();
        const juce::KeyPress nativeKey(juce::KeyPress::F1Key,
                                       juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier, 0);
        check(w.keyPressed(nativeKey), "default native timebase shortcut reaches L1 setting");
        pump();
        check(c.shuffleOptions()["mapping"] == "native", "GUI selects real undoable native Shuffle mapping");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(c.shuffleOptions()["mapping"] == "samples", "GUI mapping Undo restores legacy policy");
        check(w.keyPressed(nativeKey), "native timebase shortcut reapplies after Undo");
        pump();
        w.uiCommands().invokeDirectly(editCommand::shuffle, false);
        pump();
        run(c, Json::array({operation("session.range.set",
                                      {{"start_samples", 9 * 48000}, {"end_samples", 9 * 48000 + 24000}})}));
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
        check(w.keyPressed(custom), "custom Paste key reaches new native Shuffle policy");
        pump();
        check(!AudioDeviceTestAccess::pending(w).is_null() &&
                  AudioDeviceTestAccess::pending(w)["operations"][0]["args"]["ripple_mapping"] == "native" &&
                  AudioDeviceTestAccess::preview(w).contains(juce::String::fromUTF8("各轨实际秒位移可能不同")),
              "GUI previews actual per-track clocks and differing second offsets");
        AudioDeviceTestAccess::confirm(w, false);
        pump();
        check(persistentTracks(c) == guiBefore && c.clipboard() == b,
              "native preview cancellation preserves project and clipboard");
        check(w.keyPressed(custom), "custom key replans native preview");
        pump();
        AudioDeviceTestAccess::confirm(w, true);
        pump();
        const auto guiFile = dir.getChildFile("NativeShuffleAccepted.tracktionedit");
        c.save(guiFile);
        const auto guiAfter = persistentTracks(c);
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(persistentTracks(c) == guiBefore, "GUI acceptance is one native Undo");
        w.openSession(guiFile);
        pump();
        check(sameSavedState(persistentTracks(c), guiAfter) && c.shuffleOptions()["mapping"] == "native" &&
                  keys->containsMapping(editCommand::paste, custom),
              "GUI project mode and custom shortcut survive Open");
        for (const bool cutting : {false, true})
        {
            open();
            b = copy();
            check(w.keyPressed(nativeKey), "native setting key dispatches before range Clear");
            pump();
            w.uiCommands().invokeDirectly(editCommand::shuffle, false);
            pump();
            run(c, Json::array(
                       {operation("session.range.set", {{"start_samples", 9 * 48000}, {"end_samples", 10 * 48000}})}));
            AudioDeviceTestAccess::select(w, target[0]);
            c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", target}}, c.sessionToken());
            pump();
            const auto clearBefore = persistentTracks(c);
            const int64_t expectedMidiStart = std::llround(mapped(16., beats(9.) - beats(10.)) * 48000.);
            if (cutting)
                key(w, 'x');
            else
            {
                check(w.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey)),
                      "default Delete key dispatches native musical Shuffle");
                pump();
            }
            check(!AudioDeviceTestAccess::pending(w).is_null() &&
                      AudioDeviceTestAccess::pending(w)["operations"][0]["args"]["ripple_mapping"] == "native" &&
                      AudioDeviceTestAccess::preview(w).contains(juce::String::fromUTF8("后方曲线按拍位移")),
                  "range Cut Delete preview uses actual selected native timebase");
            AudioDeviceTestAccess::confirm(w, true);
            pump();
            check(clip(c, lateMidi)["start_samples"] == expectedMidiStart &&
                      clip(c, lateAudio)["start_samples"] == 15 * 48000,
                  "GUI range Clear shifts audio by samples and musical MIDI by actual beats");
            if (cutting)
                check(c.clipboard()["start_samples"] == 9 * 48000 && c.clipboard()["tracks"] == target &&
                          c.query().at("time_selection").is_null(),
                      "native CmdX accepts new clipboard and clears range only after committed receipt");
            else
                check(c.clipboard() == b, "native Delete retains accepted clipboard");
            const auto clearAfter = persistentTracks(c);
            const auto clearFile =
                dir.getChildFile(cutting ? "GuiMusicalCut.tracktionedit" : "GuiMusicalDelete.tracktionedit");
            c.save(clearFile);
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(persistentTracks(c) == clearBefore, "GUI range Clear is one native Undo after Save");
            w.openSession(clearFile);
            pump();
            check(sameSavedState(persistentTracks(c), clearAfter) && c.shuffleOptions()["mapping"] == "native",
                  "native GUI Cut Delete state survives Save Open");
        }
        w.openSession(guiFile);
        pump();
        // Owned corrupt-state input must not replace the current Edit.
        auto corrupt = juce::XmlDocument::parse(guiFile);
        auto* metadata = corrupt->getChildByName("NATIVEDAW");
        check(metadata != nullptr && metadata->getChildByName("SHUFFLE_OPTIONS"),
              "native saved Shuffle settings exist");
        metadata->getChildByName("SHUFFLE_OPTIONS")->setAttribute("mapping", "invented");
        const auto bad = dir.getChildFile("InvalidMapping.tracktionedit");
        check(corrupt->writeTo(bad), "owned invalid mapping fixture written");
        rejects([&] { c.open(bad); }, "invalid saved mapping rejects candidate Edit");
        check(sameSavedState(persistentTracks(c), guiAfter), "invalid mapping Open retains current actual Edit");
        open();
        w.uiCommands().invokeDirectly(editCommand::shuffleNative, false);
        pump();
        w.uiCommands().invokeDirectly(editCommand::shuffle, false);
        pump();
        keys->clearAllKeyPresses(editCommand::paste);
        keys->addKeyPress(editCommand::paste, custom);
        pump();
        run(c,
            Json::array({operation("session.range.set", {{"start_samples", 2 * 48000}, {"end_samples", 3 * 48000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", source}}, c.sessionToken());
        pump();
        const auto demo = dir.getChildFile("MusicalShuffleReady.tracktionedit");
        c.save(demo);
        check(Commands::mediaHash(media) == mediaHash && Commands::mediaHash(lateMedia) == mediaHash &&
                  juce::SHA256(fixture).toHexString() == fixtureHash,
              "all real edits preserve original media and fixture bytes");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"cases", cases},
                    {"native_iterator_probes", probes},
                    {"maximum_curve_error", maximumError},
                    {"maximum_pcm_error", maximumPcm},
                    {"maximum_saved_derived_beat_error", maximumSavedBeatRounding},
                    {"demo", demo.getFullPathName().toStdString()},
                    {"limits",
                     "native musical MIDI suffix across Tempo/ramp/Meter; sample-synced MIDI crossing Tempo and "
                     "same-track shared mixed-clock curves remain refused; physical GUI/listening unexecuted"}};
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
