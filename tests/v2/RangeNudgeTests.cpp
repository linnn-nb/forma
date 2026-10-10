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
    static const te::TempoSequence& tempo(Commands& c)
    {
        return c.edit->tempoSequence;
    }
    static te::AutomatableParameter* parameter(Commands& c, const std::string& t)
    {
        return c.automationParameter(t, "volume");
    }
    static void refresh(Workspace& w)
    {
        w.refresh();
    }
    static juce::String status(Workspace& w)
    {
        return w.status.getText();
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
template <class F> void refuses(F fn, const char* why)
{
    bool rejected = false;
    try
    {
        fn();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    check(rejected, why);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(70);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : te::PropertyStorage("Forma range Nudge tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
Json run(Commands& c, Json ops)
{
    auto r = c.commit(c.makePlan("human", std::move(ops)));
    pump();
    check(r["state"] == "committed", "actual native transaction committed");
    return r;
}
Json clipFact(Commands& c, const std::string& id)
{
    const auto facts = c.query();
    for (const auto& t : facts["tracks"])
        for (const auto& a : t["clips"])
            if (a["id"] == id)
                return a;
    throw std::runtime_error("actual clip missing");
}
juce::XmlElement* byID(juce::XmlElement& tree, const std::string& id)
{
    if (tree.getStringAttribute("id").toStdString() == id)
        return &tree;
    for (auto* child : tree.getChildIterator())
        if (auto* found = byID(*child, id))
            return found;
    return nullptr;
}
std::string hash(const juce::ValueTree& state)
{
    juce::MemoryOutputStream stream;
    state.writeToStream(stream);
    return juce::SHA256(stream.getData(), stream.getDataSize()).toHexString().toStdString();
}
Json events(Commands& c, const std::string& id)
{
    auto& clip = AudioDeviceTestAccess::clip(c, id);
    Json r = Json::array();
    for (auto* n : clip.getSequence().getNotes())
        r.push_back({{"kind", "note"},
                     {"a", n->getEditStartTime(clip).inSeconds()},
                     {"b", n->getEditEndTime(clip).inSeconds()},
                     {"pitch", n->getNoteNumber()},
                     {"velocity", n->getVelocity()}});
    for (auto* n : clip.getSequence().getControllerEvents())
        r.push_back({{"kind", "CC"},
                     {"a", n->getEditTime(clip).inSeconds()},
                     {"b", n->getEditTime(clip).inSeconds()},
                     {"type", n->getType()},
                     {"value", n->getControllerValue()},
                     {"metadata", n->getMetadata()}});
    for (auto* n : clip.getSequence().getSysexEvents())
        r.push_back(
            {{"kind", "SysEx"}, {"a", n->getEditTime(clip).inSeconds()}, {"b", n->getEditTime(clip).inSeconds()}});
    return r;
}
void sameEvents(Commands& c, const std::string& id, const Json& expected)
{
    auto actual = events(c, id);
    check(actual.size() == expected.size(), "actual complete note CC SysEx collection retained");
    for (size_t i = 0; i < expected.size(); ++i)
    {
        auto a = actual[i], e = expected[i];
        for (const char* key : {"a", "b"})
        {
            const double got = a[key], want = e[key];
            check(std::abs(got - want) <= 1e-9 && std::llround(got * 48000.) == std::llround(want * 48000.),
                  "actual event clock respects predeclared 1ns and exact rounded-sample budgets");
            a.erase(key);
            e.erase(key);
        }
        check(a == e, "native event payload retained");
    }
}
void range(Workspace& w, Commands& c, int64_t first, int64_t last, Json tracks)
{
    run(c, Json::array({op("session.range.set", {{"start_samples", first}, {"end_samples", last}}),
                        op("session.insertion.set", {{"position_samples", first}})}));
    c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", tracks}, {"edit_tool", "selector"}},
                    c.sessionToken());
    AudioDeviceTestAccess::refresh(w);
    pump();
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
void click(Workspace& w, const char* id)
{
    auto* button = dynamic_cast<juce::Button*>(find(w, id));
    check(button && button->isEnabled(), "actual preview button enabled");
    struct Delivery : juce::Button::Listener
    {
        int count = 0;
        void buttonClicked(juce::Button*) override
        {
            ++count;
        }
    } delivery;
    button->addListener(&delivery);
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    button->triggerClick();
    while (!delivery.count && std::chrono::steady_clock::now() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    button->removeListener(&delivery);
    check(delivery.count == 1, "single preview notification within 500ms");
    pump();
}
double curveBudget(te::AutomatableParameter& p, float want)
{
    const double ulp = std::max(double(std::nextafter(want, std::numeric_limits<float>::infinity())) - want,
                                double(want) - std::nextafter(want, -std::numeric_limits<float>::infinity()));
    return (p.valueRange.end - p.valueRange.start) * 1e-7 + 2 * ulp;
}
float value(te::AutomatableParameter& p, double at)
{
    te::AutomationIterator it(p);
    it.setPosition(tracktion::TimePosition::fromSeconds(at));
    return it.getCurrentValue();
}
int onset(Commands& c, const juce::File& output)
{
    auto r = c.render(output, 0, 9 * 48000);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(output));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == 9 * 48000 &&
              r["frames"] == 9 * 48000,
          "actual WAV independently decoded format and duration");
    juce::AudioBuffer<float> audio(2, 9 * 48000);
    check(reader->read(&audio, 0, audio.getNumSamples(), 0, true, true), "actual PCM read");
    for (int i = 0; i < audio.getNumSamples(); ++i)
        if (std::abs(audio.getSample(0, i)) > 3e-6)
            return i;
    throw std::runtime_error("native instrument render silent");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto folder = juce::File(juce::String::fromUTF8(argc > 2 ? argv[2] : "/tmp/forma-range-nudge"))
                                .getChildFile("range-nudge-" + juce::Uuid().toString());
        folder.createDirectory();
        Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
        w.setSize(1440, 900);
        w.setVisible(true);
        pump();
        auto& c = AudioDeviceTestAccess::owner(w);
        const auto media = folder.getChildFile("source.wav");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = format.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
        check(bool(writer), "owned real 44.1k PCM writer created");
        juce::AudioBuffer<float> signal(2, 44100);
        for (int i = 0; i < signal.getNumSamples(); ++i)
            for (int ch = 0; ch < 2; ++ch)
                signal.setSample(ch, i,
                                 .04f * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 659 : 431) * i / 44100.));
        check(writer->writeFromAudioSampleBuffer(signal, 0, signal.getNumSamples()), "real PCM fixture written");
        writer.reset();
        const auto mediaHash = Commands::mediaHash(media);
        run(c, Json::array(
                   {op("tempo.event.create", {{"beat_position", 8.}, {"bpm", 80.}}),
                    op("tempo.event.create", {{"beat_position", 16.}, {"bpm", 160.}}),
                    op("meter.event.create", {{"beat_position", 12.}, {"numerator", 3}, {"denominator", 4}}),
                    op("track.create", {{"name", "Range performance"}, {"type", "instrument"}, {"ref", "$m"}}),
                    op("track.create", {{"name", "Linked audio"}, {"ref", "$a"}}),
                    op("track.create", {{"name", "Silent selected track"}, {"ref", "$s"}}),
                    op("track.create", {{"name", "Unselected performance"}, {"type", "instrument"}, {"ref", "$u"}}),
                    op("midi.clip.create", {{"track", "$m"},
                                            {"name", "First complete MIDI"},
                                            {"position_samples", 96000},
                                            {"length_samples", 48000},
                                            {"ref", "$c"}}),
                    op("midi.note.add", {{"clip", "$c"},
                                         {"pitch", 64},
                                         {"velocity", 85},
                                         {"position_samples", 96000},
                                         {"length_samples", 24000}}),
                    op("midi.clip.create", {{"track", "$m"},
                                            {"name", "Second complete MIDI"},
                                            {"position_samples", 240000},
                                            {"length_samples", 48000},
                                            {"ref", "$d"}}),
                    op("midi.note.add", {{"clip", "$d"},
                                         {"pitch", 67},
                                         {"velocity", 70},
                                         {"position_samples", 252000},
                                         {"length_samples", 24000}}),
                    op("midi.clip.create", {{"track", "$u"},
                                            {"ref", "$unselected"},
                                            {"name", "Untouched"},
                                            {"position_samples", 240000},
                                            {"length_samples", 48000}}),
                    op("clip.import", {{"track", "$a"},
                                       {"path", media.getFullPathName().toStdString()},
                                       {"position_samples", 96000},
                                       {"ref", "$w"}}),
                    op("clip.trim", {{"clip", "$w"}, {"start_samples", 96000}, {"end_samples", 120000}})}));
        auto facts = c.query();
        const std::string m = facts["tracks"][0]["id"], a = facts["tracks"][1]["id"], s = facts["tracks"][2]["id"],
                          u = facts["tracks"][3]["id"];
        const std::string first = facts["tracks"][0]["clips"][0]["id"], second = facts["tracks"][0]["clips"][1]["id"],
                          audio = facts["tracks"][1]["clips"][0]["id"];
        const auto base = folder.getChildFile("Base.tracktionedit");
        c.save(base);
        auto xml = juce::XmlDocument::parse(base);
        for (const auto& id : {first, second})
        {
            auto* native = byID(*xml, id);
            auto* seq = native->getChildByName("SEQUENCE");
            seq->addChildElement(
                te::MidiControllerEvent::createControllerEvent(tracktion::BeatPosition::fromBeats(.25), 1, 90 << 7, 7)
                    .createXml()
                    .release());
            const uint8_t data[]{0x7d, 0x41};
            seq->addChildElement(te::MidiSysexEvent::createSysexEvent(juce::MidiMessage::createSysExMessage(data, 2),
                                                                      tracktion::BeatPosition::fromBeats(.5))
                                     .createXml()
                                     .release());
            for (auto* event : seq->getChildIterator())
                event->setAttribute("opaque_range_test", "retain");
        }
        const auto fixture = folder.getChildFile("Step.tracktionedit");
        check(xml->writeTo(fixture), "actual note CC SysEx fixture saved");
        auto rampXml = juce::XmlDocument::parse(fixture);
        for (auto* event : rampXml->getChildByName("TEMPOSEQUENCE")->getChildIterator())
            if (event->hasTagName("TEMPO") && event->getDoubleAttribute("startBeat") == 8)
                event->setAttribute("curve", .35);
        const auto ramp = folder.getChildFile("Ramp.tracktionedit");
        check(rampXml->writeTo(ramp), "actual ramp Tempo fixture saved");
        auto open = [&](const juce::File& file)
        {
            w.openSession(file);
            pump();
        };
        int matrix = 0;
        for (const auto& file : {fixture, ramp})
            for (const char* basis : {"beats", "samples"})
                for (const char* amount : {"sample", "10ms", "100ms", "beat", "quarter-beat"})
                    for (int direction : {-1, 1})
                    {
                        open(file);
                        for (const auto& id : {first, second})
                            run(c, Json::array({op("midi.clip.timebase.set", {{"clip", id}, {"basis", basis}})}));
                        range(w, c, 48000, 336000, Json::array({m, a, s}));
                        c.updateUiState({{"nudge", amount}}, c.sessionToken());
                        AudioDeviceTestAccess::refresh(w);
                        const auto before = c.query();
                        std::map<std::string, Json> expected;
                        std::map<std::string, juce::ValueTree> states;
                        const int64_t delta =
                            std::string(amount) == "beat" || std::string(amount) == "quarter-beat"
                                ? c.offsetByBeats(48000, direction * (std::string(amount) == "beat" ? 1. : .25)) - 48000
                                : direction * int64_t(std::string(amount) == "sample" ? 1
                                                      : std::string(amount) == "10ms" ? 480
                                                                                      : 4800);
                        const auto& tempo = AudioDeviceTestAccess::tempo(c).getInternalSequence();
                        for (const auto& id : {first, second})
                        {
                            expected[id] = events(c, id);
                            states[id] = AudioDeviceTestAccess::clip(c, id).state.createCopy();
                            const auto original = clipFact(c, id);
                            const double beatDelta =
                                tempo
                                    .toBeats(tracktion::TimePosition::fromSeconds(
                                        (original["start_samples"].get<int64_t>() + delta) / 48000.))
                                    .inBeats() -
                                original["start_beat"].get<double>();
                            for (auto& event : expected[id])
                                for (const char* key : {"a", "b"})
                                    event[key] =
                                        std::string(basis) == "samples"
                                            ? event[key].get<double>() + delta / 48000.
                                            : tempo
                                                  .toTime(tracktion::BeatPosition::fromBeats(
                                                      tempo.toBeats(tracktion::TimePosition::fromSeconds(event[key]))
                                                          .inBeats() +
                                                      beatDelta))
                                                  .inSeconds();
                        }
                        check(w.uiCommands().invokeDirectly(
                                  direction < 0 ? editCommand::nudgeBack : editCommand::nudgeForward, false),
                              "actual global range Nudge dispatched");
                        pump();
                        const auto after = c.query();
                        std::cout << "CASE " << file.getFileName() << " " << basis << " " << amount << " " << direction
                                  << " before=" << before["revision"] << " after=" << after["revision"]
                                  << " status=" << AudioDeviceTestAccess::status(w) << std::endl;
                        check(after["revision"].get<uint64_t>() == before["revision"].get<uint64_t>() + 1,
                              "complete mixed range committed as one revision");
                        check(c.timelineRange()["start_samples"] == 48000 + delta &&
                                  c.timelineRange()["end_samples"] == 336000 + delta &&
                                  after["position_samples"] == 48000 + delta,
                              "complete silent envelope and insertion shifted together");
                        check(clipFact(c, audio)["start_samples"] == 96000 + delta &&
                                  clipFact(c, audio)["length_samples"] == 24000 &&
                                  after["tracks"][3] == before["tracks"][3],
                              "real audio and stable unselected track preserved");
                        for (const auto& id : {first, second})
                        {
                            sameEvents(c, id, expected[id]);
                            const auto source = states[id].getChildWithName("SEQUENCE");
                            auto& native = AudioDeviceTestAccess::clip(c, id);
                            if (std::string(basis) == "beats")
                                check(hash(source) == hash(native.getSequence().state),
                                      "musical raw SEQ and unknown metadata retained");
                            else
                            {
                                sample_midi::validateOrigin(native.state);
                                check(native.state.getChildWithName("NDAW_SAMPLE_MIDI_ORIGIN")
                                              .getProperty("source_hash")
                                              .toString()
                                              .toStdString() == hash(source),
                                      "absolute mapping keeps exact original sequence provenance");
                            }
                        }
                        const auto saved = folder.getChildFile("Matrix-" + juce::String(matrix) + ".tracktionedit");
                        c.save(saved);
                        c.undo();
                        pump();
                        check(c.timelineRange() == before["time_selection"], "one Undo restores original envelope");
                        for (const auto& id : {first, second})
                            check(AudioDeviceTestAccess::clip(c, id).state.isEquivalentTo(states[id]),
                                  "one native Undo restores entire MIDI state and identity");
                        c.redo();
                        pump();
                        for (const auto& id : {first, second})
                            sameEvents(c, id, expected[id]);
                        open(saved);
                        for (const auto& id : {first, second})
                            sameEvents(c, id, expected[id]);
                        check(c.timelineRange() == after["time_selection"] && Commands::mediaHash(media) == mediaHash,
                              "native reopen preserves range and original PCM hash");
                        ++matrix;
                    }
        // The complete selected envelope carries automation through silence and empty tracks.
        double worst = 0;
        for (const auto& file : {fixture, ramp})
            for (const char* clock : {"samples", "beats"})
            {
                open(file);
                for (const auto& owner : {m, a, s})
                {
                    run(c, Json::array({op("track.automation_edit_basis.set", {{"track", owner}, {"basis", clock}})}));
                    Json points = Json::array();
                    for (int i = 0; i < 10; ++i)
                        points.push_back(op("automation.point.add", {{"track", owner},
                                                                     {"parameter", "volume"},
                                                                     {"ref", "$p" + std::to_string(i)},
                                                                     {"position_samples", 24000 + i * 48000},
                                                                     {"value", double(i % 3 ? -12 : -3)},
                                                                     {"curve", i % 2 ? .75 : -.75}}));
                    run(c, points);
                }
                range(w, c, 48000, 336000, Json::array({m, a, s}));
                const auto old = c.query();
                const auto& tempo = AudioDeviceTestAccess::tempo(c).getInternalSequence();
                const double beatDelta = tempo.toBeats(tracktion::TimePosition::fromSeconds(1.1)).inBeats() -
                                         tempo.toBeats(tracktion::TimePosition::fromSeconds(1.)).inBeats();
                std::map<std::string, std::vector<std::pair<double, float>>> samples;
                std::map<std::string, juce::ValueTree> curves;
                std::map<std::string, float> bases;
                for (const auto& owner : {m, a, s})
                {
                    auto* p = AudioDeviceTestAccess::parameter(c, owner);
                    curves[owner] = p->getCurve().state.createCopy();
                    bases[owner] = p->getCurrentExplicitValue();
                    for (int i = 0; i < 512; ++i)
                    {
                        const double source = 1.01 + i * 5.97 / 512.;
                        const double target =
                            std::string(clock) == "samples"
                                ? source + .1
                                : tempo
                                      .toTime(tracktion::BeatPosition::fromBeats(
                                          tempo.toBeats(tracktion::TimePosition::fromSeconds(source)).inBeats() +
                                          beatDelta))
                                      .inSeconds();
                        samples[owner].emplace_back(target, value(*p, source));
                    }
                }
                auto plan = c.makeRangeNudgePlan(Json::array({m, a, s}), 48000, 336000, 4800);
                auto preview = c.preview(plan);
                check(preview["automation_changes"].size() == 3,
                      "full envelope preview includes empty selected track curves");
                auto receipt = c.commit(plan);
                pump();
                check(receipt["state"] == "committed" && c.commit(plan)["replayed"] == true,
                      "range Nudge retry idempotent");
                for (const auto& owner : {m, a, s})
                {
                    auto* p = AudioDeviceTestAccess::parameter(c, owner);
                    for (const auto& [at, want] : samples[owner])
                    {
                        const double error = std::abs(value(*p, at) - want);
                        worst = std::max(worst, error);
                        check(error <= curveBudget(*p, want),
                              "actual native DSP curve through clip gaps meets declared span1e-7 plus float budget");
                    }
                    check(p->getCurrentExplicitValue() == bases[owner],
                          "explicit base independent of transported automation");
                }
                const auto saved = folder.getChildFile("Curves-" + juce::String(clock) + file.getFileName());
                c.save(saved);
                c.undo();
                pump();
                check(c.timelineRange() == old["time_selection"], "curve Undo includes full envelope");
                for (const auto& owner : {m, a, s})
                    check(AudioDeviceTestAccess::parameter(c, owner)->getCurve().state.isEquivalentTo(curves[owner]),
                          "single Undo restores actual full native curve IDs and fields");
                c.redo();
                pump();
                open(saved);
                for (const auto& owner : {m, a, s})
                    for (const auto& [at, want] : samples[owner])
                        check(std::abs(value(*AudioDeviceTestAccess::parameter(c, owner), at) - want) <=
                                  curveBudget(*AudioDeviceTestAccess::parameter(c, owner), want),
                              "reopened native playback curve retains measured mapping");
            }
        open(fixture);
        range(w, c, 48000, 336000, Json::array({m, a, s}));
        run(c, Json::array({op("group.create", {{"id", "range-linked"},
                                                {"name", "Mixed linked"},
                                                {"members", Json::array({m, a, s})},
                                                {"enabled", true},
                                                {"edit", true},
                                                {"mute", false},
                                                {"solo", false}})}));
        auto before = c.query();
        auto grouped = c.makeRangeNudgePlan(Json::array({a}), 48000, 336000, 480);
        check(grouped["operations"].size() == 5, "group closure deduplicates two MIDI and one audio moves");
        c.commit(grouped);
        pump();
        check(clipFact(c, first)["start_samples"] == 96480 && clipFact(c, second)["start_samples"] == 240480 &&
                  clipFact(c, audio)["start_samples"] == 96480,
              "group-seeded actual mixed clips move once");
        c.undo();
        pump();
        refuses([&] { c.makeRangeNudgePlan(Json::array({a}), 48000, 124800, 480); },
                "partial group peer refuses complete Plan before mutation");
        check(c.query()["tracks"] == before["tracks"], "partial peer refusal preserves entire group");
        range(w, c, 100000, 110000, Json::array({m, a}));
        before = c.query();
        c.commit(c.makeRangeNudgePlan(Json::array({m, a}), 100000, 110000, 480));
        pump();
        check(c.query()["tracks"] == before["tracks"] && c.timelineRange()["start_samples"] == 100480,
              "only-partial range moves selection without implicit MIDI/audio cut");
        c.undo();
        pump();
        auto plan = c.makeRangeNudgePlan(Json::array({m}), 48000, 336000, 480);
        auto forged = plan;
        forged["operations"][0]["args"]["position_samples"] = 123456;
        refuses([&] { c.commit(forged); }, "descriptor recompilation rejects forged target");
        forged = plan;
        forged["actor"] = "agent:range";
        refuses([&] { c.commit(forged); }, "external actor cannot adopt local human envelope planner");
        run(c, Json::array({op("track.gain", {{"track", a}, {"db", -9.}})}));
        before = c.query();
        refuses([&] { c.commit(plan); }, "interleaved human change invalidates planned Nudge");
        check(c.query()["tracks"] == before["tracks"] && c.query()["revision"] == before["revision"],
              "stale refusal leaves newer human state");
        refuses([&] { c.makeRangeNudgePlan(Json::array({m}), 100, 1000, -480); },
                "negative full envelope refused before any edit");
        refuses([&] { c.makeRangeNudgePlan(Json::array({m, m}), 48000, 336000, 480); },
                "duplicate input tracks refused");
        auto overflow = c.makeRangeNudgePlan(Json::array({m}), 48000, 336000, -1);
        overflow["timeline_range_nudge"]["delta_samples"] = std::numeric_limits<uint64_t>::max();
        refuses([&] { c.preview(overflow); }, "unsigned overflow cannot masquerade as signed minus-one Nudge");
        // Shared mixed clocks only require an explicit choice when an actual curve exists.
        open(fixture);
        run(c, Json::array({op(
                   "clip.import",
                   {{"track", m}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 192000}})}));
        auto noCurve = c.makeRangeNudgePlan(Json::array({m}), 48000, 336000, 480);
        check(noCurve["operations"].size() == 5, "mixed clocks without a curve remain fully usable");
        auto r = c.commit(noCurve);
        pump();
        check(r["state"] == "committed", "mixed-clock no-curve Nudge actually commits");
        c.undo();
        pump();
        run(c, Json::array({op("automation.point.add", {{"track", m},
                                                        {"parameter", "volume"},
                                                        {"ref", "$point"},
                                                        {"position_samples", 180000},
                                                        {"value", -9.},
                                                        {"curve", .5}}),
                            op("automation.point.add", {{"track", m},
                                                        {"parameter", "volume"},
                                                        {"ref", "$point2"},
                                                        {"position_samples", 240000},
                                                        {"value", -3.},
                                                        {"curve", -.5}})}));
        before = c.query();
        refuses([&] { c.makeRangeNudgePlan(Json::array({m}), 48000, 336000, 480); },
                "ambiguous mixed shared curve requires explicit basis");
        check(c.query()["revision"] == before["revision"] && c.query()["tracks"] == before["tracks"],
              "ambiguous clock refusal preserves all actual state");
        run(c, Json::array({op("track.automation_edit_basis.set", {{"track", m}, {"basis", "samples"}})}));
        auto sealed = c.makeRangeNudgePlan(Json::array({m}), 48000, 336000, 480);
        check(c.preview(sealed)["automation_changes"].size() == 1,
              "explicit mixed shared clock yields one full-range curve change");
        auto altered = sealed;
        altered["operations"][0]["args"]["state_hash"] = "forged";
        refuses([&] { c.commit(altered); }, "forged range-curve seal refused");
        altered = sealed;
        altered.erase("timeline_range_nudge");
        refuses([&] { c.commit(altered); }, "derived range curve cannot bypass its compiler descriptor");
        c.commit(sealed);
        pump();
        c.undo();
        pump();
        run(c, Json::array({op("session.automation_follows_edit.set", {{"enabled", false}})}));
        auto* parameter = AudioDeviceTestAccess::parameter(c, m);
        const auto curve = parameter->getCurve().state.createCopy();
        auto off = c.makeRangeNudgePlan(Json::array({m}), 48000, 336000, 480);
        check(c.preview(off)["automation_changes"].empty(),
              "follow-off preview honestly reports no automation changes");
        c.commit(off);
        pump();
        check(parameter->getCurve().state.isEquivalentTo(curve), "follow-off actually retains complete native curve");
        c.undo();
        pump();
        const auto registry = Commands::registry();
        for (const auto& item : registry)
            if (item["id"] == "automation.range.move")
                check(item["tool_visibility"] == "local_gui",
                      "derived envelope operation stays hidden from frozen MCP gateway");
        // Local saved native lock/loop fixtures are data, not production direct Edit writes.
        for (const char* fault : {"ndaw_locked", "loopLengthBeats"})
        {
            auto bad = juce::XmlDocument::parse(fixture);
            auto* node = byID(*bad, first);
            node->setAttribute(fault, std::string(fault) == "ndaw_locked" ? 1 : 2);
            auto file = folder.getChildFile(juce::String(fault) + ".tracktionedit");
            check(bad->writeTo(file), "owned native failure fixture saved");
            open(file);
            before = c.query();
            refuses([&] { c.makeRangeNudgePlan(Json::array({m, a}), 48000, 336000, 480); },
                    "locked or unqualified MIDI blocks whole mixed range");
            check(c.query()["tracks"] == before["tracks"] && c.query()["revision"] == before["revision"],
                  "MIDI guard failure atomic");
        }
        // Actual production customized key survives reopen and the very first dispatch.
        open(fixture);
        range(w, c, 48000, 336000, Json::array({m}));
        c.updateUiState({{"nudge", "10ms"}}, c.sessionToken());
        const juce::KeyPress key(
            'j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        auto* mapping = w.uiCommands().getKeyMappings();
        mapping->clearAllKeyPresses(editCommand::nudgeForward);
        mapping->addKeyPress(editCommand::nudgeForward, key);
        pump();
        const auto demo = folder.getChildFile("RangeNudgeReady.tracktionedit");
        c.save(demo);
        open(demo);
        check(w.uiCommands().getKeyMappings()->containsMapping(editCommand::nudgeForward, key) &&
                  w.uiCommands().getKeyMappings()->keyPressed(key, &w),
              "first reopened customized range Nudge shortcut invokes actual command");
        pump();
        check(clipFact(c, first)["start_samples"] == 96480, "reopened custom key moves real MIDI");
        c.undo();
        pump();
        // Force real full-range preview; accept/reject do not confuse execution success.
        range(w, c, 48000, 70 * 48000, Json::array({m}));
        before = c.query();
        check(w.uiCommands().invokeDirectly(editCommand::nudgeForward, false),
              "large production range command invoked");
        pump();
        check(find(w, "plan.accept") && c.query()["revision"] == before["revision"],
              "large envelope preview does not mutate Edit");
        click(w, "plan.reject");
        check(c.query()["tracks"] == before["tracks"], "reject leaves actual clips unchanged");
        check(w.uiCommands().invokeDirectly(editCommand::nudgeForward, false),
              "large production preview proposed again");
        pump();
        click(w, "plan.accept");
        check(clipFact(c, first)["start_samples"] == 96480, "accepted preview moves actual MIDI");
        c.undo();
        pump();
        // Verify audible native FourOsc scheduling with independently decoded PCM.
        open(fixture);
        run(c, Json::array({op("track.mute", {{"track", a}, {"enabled", true}}),
                            op("track.mute", {{"track", u}, {"enabled", true}})}));
        const int originalOnset = onset(c, folder.getChildFile("Before.wav"));
        range(w, c, 48000, 336000, Json::array({m}));
        c.commit(c.makeRangeNudgePlan(Json::array({m}), 48000, 336000, 4800));
        pump();
        const int movedOnset = onset(c, folder.getChildFile("After.wav"));
        check(originalOnset == 96000 && movedOnset == 100800,
              "actual native instrument onset follows 4800-sample range Nudge");
        c.undo();
        pump();
        check(onset(c, folder.getChildFile("Undo.wav")) == originalOnset, "actual native audio Undo restores onset");
        check(Commands::mediaHash(media) == mediaHash, "original PCM unchanged across all edits");
        const Json report{{"state", "passed"},
                          {"checks", checks},
                          {"geometry_matrix", matrix},
                          {"native_curve_probes", 4 * 3 * 512},
                          {"worst_curve_error", worst},
                          {"event_seconds_budget", 1e-9},
                          {"rounded_samples", "exact"},
                          {"render_onsets", Json::array({originalOnset, movedOnset})},
                          {"demo", demo.getFullPathName().toStdString()},
                          {"physical_GUI", "not executed"},
                          {"limits", "MIDI Separate and partial-clip Nudge are distinct; implicit slicing "
                                     "intentionally absent; loops/Groove/expression not qualified"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
        }
        std::cout << report.dump(2) << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
