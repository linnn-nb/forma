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
    static te::AutomatableParameter* parameter(Commands& c, const std::string& t, const std::string& p)
    {
        return c.automationParameter(t, p);
    }
    static EditWindow& area(Workspace& w)
    {
        return w.editArea;
    }
    static void refresh(Workspace& w)
    {
        w.refresh();
    }
    static Json context(Workspace& w)
    {
        return {{"selection", w.selection.objects}, {"status", w.status.getText().toStdString()}};
    }
    static void select(Workspace& w, const std::string& id)
    {
        w.selectAudioClip(id);
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
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
void click(Workspace& w, const char* id)
{
    auto* button = dynamic_cast<juce::Button*>(find(w, id));
    check(button && button->isEnabled(), "actual production button enabled");
    struct Delivery final : juce::Button::Listener
    {
        int count = 0;
        std::chrono::steady_clock::time_point at;
        void buttonClicked(juce::Button*) override
        {
            ++count;
            at = std::chrono::steady_clock::now();
        }
    } delivery;
    button->addListener(&delivery);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    button->triggerClick();
    while (!delivery.count && std::chrono::steady_clock::now() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    button->removeListener(&delivery);
    check(delivery.count == 1 && delivery.at <= deadline,
          "single actual native button notification delivered within 500ms budget");
}
auto event(EditWindow& area, float x, int y, bool drag = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {x, float(y)},
                            juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, &area, &area, now, {x, float(y)},
                            now, 1, drag);
}

Json actualClip(Commands& c, const std::string& id)
{
    const auto facts = c.query();
    for (const auto& track : facts["tracks"])
        for (const auto& clip : track["clips"])
            if (clip["id"] == id)
                return clip;
    throw std::runtime_error("stable MIDI clip missing");
}
Json persistent(Commands& c, const std::string& id)
{
    auto lanes = c.automationQuery(id)["lanes"];
    for (auto& lane : lanes)
        for (const char* key : {"value", "explicit_value", "display", "recording"})
            lane.erase(key);
    return lanes;
}
double nativeValue(te::AutomationIterator& it, double seconds)
{
    it.setPosition(tracktion::TimePosition::fromSeconds(seconds));
    return it.getCurrentValue();
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto dir = juce::File(juce::String::fromUTF8(argc > 2 ? argv[2] : "/tmp/forma-sample-midi"))
                             .getChildFile("midi-trim-" + juce::Uuid().toString());
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

        int cases = 0;
        for (const auto& file : {fixture, ramp})
            for (const auto& basis : {"beats", "samples"})
                for (const auto& bounds : std::vector<std::pair<int64_t, int64_t>>{
                         {108000, 312000}, {72000, 384000}, {228000, 480000}, {96000, 96001}})
                {
                    open(file);
                    run(c, Json::array({op("midi.clip.timebase.set", {{"clip", src}, {"basis", basis}})}));
                    const auto before = AudioDeviceTestAccess::clip(c, src).state.createCopy();
                    const auto rawHash = treeHash(AudioDeviceTestAccess::clip(c, src).getSequence().state);
                    const auto originHash = treeHash(before.getChildWithName("NDAW_SAMPLE_MIDI_ORIGIN"));
                    const auto initial = timing(c, src);
                    const auto suffixHash = treeHash(AudioDeviceTestAccess::clip(c, suffix).state);
                    auto extent = c.midiClipTrimExtent(src, bounds.first, bounds.second, c.sessionToken(),
                                                       c.querySummary()["revision"]);
                    check(!extent.contains("state_hash"), "readonly trim draft has no stale source checksum");
                    auto plan =
                        c.makePlan("human", Json::array({op("midi.clip.trim", {{"clip", src},
                                                                               {"start_samples", bounds.first},
                                                                               {"end_samples", bounds.second}})}));
                    const auto receipt = c.commit(plan);
                    pump();
                    check(receipt["state"] == "committed" && c.commit(plan)["replayed"] == true,
                          "real trim and retry are acknowledged exactly once");
                    sameTimes(c, src, initial, 0);
                    check(actualClip(c, src)["start_samples"] == bounds.first &&
                              actualClip(c, src)["length_samples"] == bounds.second - bounds.first,
                          "native trim adopts exact canonical boundaries");
                    check(treeHash(AudioDeviceTestAccess::clip(c, src).getSequence().state) == rawHash &&
                              treeHash(AudioDeviceTestAccess::clip(c, src).state.getChildWithName(
                                  "NDAW_SAMPLE_MIDI_ORIGIN")) == originHash,
                          "complete raw SEQ opaque events and original provenance unchanged");
                    check(treeHash(AudioDeviceTestAccess::clip(c, suffix).state) == suffixHash,
                          "unselected MIDI clip untouched");
                    check(std::abs(AudioDeviceTestAccess::clip(c, src).getPosition().getOffset().inSeconds() -
                                   extent["source_offset_seconds"].get<double>()) <= 1e-12,
                          "actual offset matches native preflight");
                    check(std::abs(AudioDeviceTestAccess::clip(c, src).getOffsetInBeats().inBeats() -
                                   extent["offset_beats"].get<double>()) <= 1e-12,
                          "preview beat offset matches actual native MIDI offset");
                    check(actualClip(c, src)["minimum_start_samples"] == extent["minimum_start_samples"] &&
                              actualClip(c, src)["source_offset_samples"] == extent["source_offset_samples"],
                          "native derived source minimum and offset match readonly draft");
                    const auto saved = dir.getChildFile("Trim-" + juce::String(cases) + ".tracktionedit");
                    c.save(saved);
                    c.undo(receipt["plan_id"]);
                    pump();
                    check(AudioDeviceTestAccess::clip(c, src).state.isEquivalentTo(before),
                          "one native Undo restores complete MIDI state");
                    c.redo();
                    pump();
                    sameTimes(c, src, initial, 0);
                    open(saved);
                    sameTimes(c, src, initial, 0);
                    check(actualClip(c, src)["start_samples"] == bounds.first &&
                              actualClip(c, src)["length_samples"] == bounds.second - bounds.first,
                          "trim survives save close reopen");
                    ++cases;
                }
        {
            // Preserve a native fractional-sample boundary when only the other edge changes.
            auto xml = juce::XmlDocument::parse(fixture);
            auto* clip = byID(*xml, src);
            const double start = 4. + .25 / 48000.;
            clip->setAttribute("start", juce::String(start, 17));
            clip->setAttribute("offset", 0.);
            const auto file = dir.getChildFile("Fractional.tracktionedit");
            check(xml->writeTo(file), "owned fractional native source fixture written");
            open(file);
            const auto before = AudioDeviceTestAccess::clip(c, src).getPosition();
            const auto events = timing(c, src), facts = actualClip(c, src);
            check(facts["minimum_start_samples"] == facts["start_samples"],
                  "unchanged canonical edge remains valid even if ceil source exceeds round start");
            const int64_t first = facts["start_samples"], last = first + facts["length_samples"].get<int64_t>() - 480;
            const auto result = run(
                c,
                Json::array({op("midi.clip.trim", {{"clip", src}, {"start_samples", first}, {"end_samples", last}})}));
            check(AudioDeviceTestAccess::clip(c, src).getPosition().getStart() == before.getStart() &&
                      AudioDeviceTestAccess::clip(c, src).getPosition().getOffset() == before.getOffset(),
                  "end trim preserves untouched native fractional start and offset exactly");
            sameTimes(c, src, events, 0);
            auto saved = dir.getChildFile("FractionalTrim.tracktionedit");
            c.save(saved);
            open(saved);
            check(AudioDeviceTestAccess::clip(c, src).getPosition().getStart() == before.getStart(),
                  "native fractional boundary survives actual save reopen");
            sameTimes(c, src, events, 0);
        }
        // Track curves and explicit base parameters remain at project time under Trim.
        open(ramp);
        std::string parameter;
        const auto lanes = c.automationQuery(owner)["lanes"];
        for (const auto& lane : lanes)
            if (lane["parameter"] == "volume")
                parameter = lane["id"];
        Json points = Json::array();
        for (int i = 0; i < 8; ++i)
            points.push_back(op("automation.point.add", {{"track", owner},
                                                         {"parameter", parameter},
                                                         {"position_samples", i * 48000},
                                                         {"value", i % 2 ? -6. : -18.},
                                                         {"curve", i % 2 ? .75 : -.75},
                                                         {"ref", "$point" + std::to_string(i)}}));
        run(c, points);
        auto* actualParameter = AudioDeviceTestAccess::parameter(c, owner, parameter);
        const float baseValue = actualParameter->getCurrentExplicitValue();
        const auto curves = persistent(c, owner);
        const auto receipt = run(
            c,
            Json::array({op("midi.clip.trim", {{"clip", src}, {"start_samples", 192000}, {"end_samples", 312000}})}));
        check(persistent(c, owner) == curves && actualParameter->getCurrentExplicitValue() == baseValue,
              "Trim retains actual native curve IDs values and explicit base value");
        auto saved = dir.getChildFile("CurvedTrim.tracktionedit");
        c.save(saved);
        c.undo(receipt["plan_id"]);
        pump();
        check(persistent(c, owner) == curves, "Trim Undo leaves native curves untouched");
        c.redo();
        pump();
        open(saved);
        check(persistent(c, owner) == curves &&
                  AudioDeviceTestAccess::parameter(c, owner, parameter)->getCurrentExplicitValue() == baseValue,
              "save reopen retains native curves and explicit base value");
        // Group Audio and MIDI, both anchor directions, bounded by actual source availability.
        open(fixture);
        const auto wave = dir.getChildFile("Peer.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> stream = wave.createOutputStream();
            auto writer = format.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            juce::AudioBuffer<float> pcm(2, 240000);
            for (int i = 0; i < pcm.getNumSamples(); ++i)
                for (int ch = 0; ch < 2; ++ch)
                    pcm.setSample(ch, i, .05f * std::sin(float(i) * .04f));
            check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()),
                  "actual owned group PCM written");
        }
        run(c, Json::array({op("track.create", {{"name", "Audio peer"}, {"type", "audio"}, {"ref", "$peer"}}),
                            op("clip.import", {{"track", "$peer"},
                                               {"path", wave.getFullPathName().toStdString()},
                                               {"position_samples", 96000},
                                               {"ref", "$peerclip"}})}));
        query = c.query();
        const std::string peer = query["tracks"][2]["id"], peerClip = query["tracks"][2]["clips"][0]["id"];
        run(c, Json::array({op("group.create", {{"id", "trim-group"},
                                                {"name", "Audio MIDI edges"},
                                                {"members", Json::array({owner, peer})},
                                                {"enabled", true},
                                                {"edit", true},
                                                {"mute", false},
                                                {"solo", false}})}));
        const auto groupFile = dir.getChildFile("MixedGroup.tracktionedit");
        c.save(groupFile);
        const auto mediaHash = Commands::mediaHash(wave);
        for (bool midiAnchor : {true, false})
        {
            open(groupFile);
            const auto initial = timing(c, src);
            const auto result = run(
                c, Json::array({op(
                       midiAnchor ? "midi.clip.trim" : "clip.trim",
                       {{"clip", midiAnchor ? src : peerClip}, {"start_samples", 144000}, {"end_samples", 288000}})}));
            check(actualClip(c, src)["start_samples"] == 144000 && actualClip(c, peerClip)["start_samples"] == 144000 &&
                      actualClip(c, src)["length_samples"] == 144000 &&
                      actualClip(c, peerClip)["length_samples"] == 144000,
                  "audio and MIDI anchors both trim the actual mixed Edit group");
            sameTimes(c, src, initial, 0);
            auto file = dir.getChildFile(midiAnchor ? "GroupMidi.tracktionedit" : "GroupAudio.tracktionedit");
            c.save(file);
            c.undo(result["plan_id"]);
            pump();
            check(actualClip(c, src)["start_samples"] == 96000 && actualClip(c, peerClip)["length_samples"] == 240000,
                  "single native Undo restores whole mixed group");
            c.redo();
            pump();
            open(file);
            check(actualClip(c, src)["start_samples"] == 144000 && actualClip(c, peerClip)["length_samples"] == 144000,
                  "mixed group trim survives save reopen");
        }
        open(groupFile);
        const auto guarded = c.query();
        rejects(
            [&]
            {
                c.makePlan("human",
                           Json::array({op("midi.clip.trim",
                                           {{"clip", src}, {"start_samples", 72000}, {"end_samples", 336000}})}));
            },
            "MIDI source reveal cannot exceed grouped audio source");
        check(c.query() == guarded && Commands::mediaHash(wave) == mediaHash,
              "source bound refusal is atomic and PCM remains unchanged");
        {
            auto locked = juce::XmlDocument::parse(groupFile);
            byID(*locked, src)->setAttribute("ndaw_locked", 1);
            const auto file = dir.getChildFile("LockedMixedGroup.tracktionedit");
            check(locked->writeTo(file), "owned locked mixed trim fixture written");
            open(file);
            const auto before = c.query();
            rejects(
                [&]
                {
                    c.makePlan(
                        "human",
                        Json::array({op("clip.trim",
                                        {{"clip", peerClip}, {"start_samples", 144000}, {"end_samples", 288000}})}));
                },
                "audio trim anchor cannot bypass locked MIDI peer");
            check(c.query() == before, "mixed group lock refusal is atomic");
        }
        for (const auto& mode : {"locked", "loop", "expression"})
        {
            auto bad = juce::XmlDocument::parse(fixture);
            auto* clip = byID(*bad, src);
            if (std::string(mode) == "locked")
                clip->setAttribute("ndaw_locked", 1);
            else if (std::string(mode) == "loop")
                clip->setAttribute("loopLengthBeats", 2.);
            else
                clip->getChildByName("SEQUENCE")
                    ->getChildByName("NOTE")
                    ->addChildElement(new juce::XmlElement("EXPRESSION"));
            const auto file = dir.getChildFile(juce::String(mode) + ".tracktionedit");
            check(bad->writeTo(file), "owned unqualified MIDI trim fixture saved");
            open(file);
            const auto before = c.query();
            rejects(
                [&]
                {
                    c.makePlan("human",
                               Json::array({op("midi.clip.trim",
                                               {{"clip", src}, {"start_samples", 108000}, {"end_samples", 312000}})}));
                },
                "locked loop or expression trim rejected before writes");
            check(c.query() == before, "unsupported MIDI trim causes no partial state or revision");
        }
        // All four edge Nudge commands and five units share production dispatch.
        open(fixture);
        for (const auto& basis : {"beats", "samples"})
        {
            run(c, Json::array({op("midi.clip.timebase.set", {{"clip", src}, {"basis", basis}})}));
            for (const auto& unit : {"sample", "10ms", "100ms", "beat", "quarter-beat"})
                for (const int command : {editCommand::trimStartBack, editCommand::trimStartForward,
                                          editCommand::trimEndBack, editCommand::trimEndForward})
                {
                    c.updateUiState({{"nudge", unit}, {"edit_mode", "slip"}}, c.sessionToken());
                    AudioDeviceTestAccess::refresh(w);
                    AudioDeviceTestAccess::select(w, src);
                    pump();
                    const auto initial = timing(c, src);
                    const bool first = command <= editCommand::trimStartForward;
                    const int direction =
                        command == editCommand::trimStartBack || command == editCommand::trimEndBack ? -1 : 1;
                    const int64_t anchor = first ? 96000 : 336000;
                    const int64_t delta =
                        std::string(unit) == "sample" ? direction
                        : std::string(unit) == "10ms" ? 480 * direction
                        : std::string(unit) == "100ms"
                            ? 4800 * direction
                            : c.offsetByBeats(anchor, direction * (std::string(unit) == "beat" ? 1 : .25)) - anchor;
                    check(w.uiCommands().invokeDirectly(command, false), "production MIDI edge Nudge enabled");
                    pump();
                    check(actualClip(c, src)["start_samples"] == (first ? 96000 + delta : 96000) &&
                              actualClip(c, src)["length_samples"] == 240000 + (first ? -delta : delta),
                          "production edge Nudge has exact native bounds");
                    sameTimes(c, src, initial, 0);
                    w.uiCommands().invokeDirectly(6, false);
                    pump();
                    check(actualClip(c, src)["start_samples"] == 96000 &&
                              actualClip(c, src)["length_samples"] == 240000,
                          "edge Nudge uses one native Undo");
                }
        }
        // Actual Trim tool mouse draft/release, not a private GUI write.
        auto& area = AudioDeviceTestAccess::area(w);
        c.updateUiState({{"start_samples", 0},
                         {"span_samples", int64_t(area.coordinates().width) * 480},
                         {"edit_tool", "trim"},
                         {"edit_mode", "slip"}},
                        c.sessionToken());
        AudioDeviceTestAccess::refresh(w);
        pump();
        for (bool first : {true, false})
        {
            const auto original = timing(c, src), facts = actualClip(c, src);
            const auto rect = area.clipRect(facts, 0);
            const float x = float(first ? rect.getX() + 4 : rect.getRight() - 4);
            const int64_t at = first ? 120000 : 312000;
            const float targetX = float(area.coordinates().pixelAt(at));
            const int y = rect.getCentreY();
            area.mouseDown(event(area, x, y));
            area.mouseDrag(event(area, targetX, y, true));
            juce::Image image(juce::Image::ARGB, area.getWidth(), area.getHeight(), true);
            juce::Graphics graphics(image);
            area.paint(graphics);
            area.mouseUp(event(area, targetX, y, true));
            pump();
            check(actualClip(c, src)["start_samples"] == (first ? at : 96000) &&
                      actualClip(c, src)["length_samples"] == (first ? 336000 - at : at - 96000),
                  "actual Trim mouse release commits MIDI edge");
            sameTimes(c, src, original, 0);
            w.uiCommands().invokeDirectly(6, false);
            pump();
        }
        AudioDeviceTestAccess::select(w, src);
        pump();
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom(
            'k', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        keys->clearAllKeyPresses(editCommand::trimEndBack);
        keys->addKeyPress(editCommand::trimEndBack, custom);
        c.updateUiState({{"nudge", "10ms"}}, c.sessionToken());
        AudioDeviceTestAccess::refresh(w);
        pump();
        check(w.keyPressed(custom), "custom MIDI boundary key dispatched");
        pump();
        check(actualClip(c, src)["length_samples"] == 239520, "custom boundary key actually trims native MIDI");
        const auto demo = dir.getChildFile("MidiTrimReady.tracktionedit");
        c.save(demo);
        open(demo);
        AudioDeviceTestAccess::select(w, src);
        pump();
        check(w.keyPressed(custom), "custom boundary shortcut survives reopen");
        pump();
        check(actualClip(c, src)["length_samples"] == 239040, "first reopened boundary key performs real edit");
        // Validation and stale plans.
        open(fixture);
        for (auto args : std::vector<Json>{
                 {{"clip", src}, {"start_samples", 71999}, {"end_samples", 336000}},
                 {{"clip", src}, {"start_samples", 96000.5}, {"end_samples", 336000}},
                 {{"clip", src}, {"start_samples", 96000}, {"end_samples", 96000}},
                 {{"clip", src}, {"start_samples", 96000}, {"end_samples", 336000}, {"unsupported", true}},
                 {{"clip", src}, {"start_samples", 96000}, {"end_samples", 312000}, {"state_hash", "forged"}}})
        {
            const auto before = c.query();
            rejects([&] { c.makePlan("human", Json::array({op("midi.clip.trim", args)})); },
                    "invalid source boundary type unknown field or stale hash refused");
            check(c.query() == before, "invalid trim causes no native state or revision changes");
        }
        rejects(
            [&]
            {
                c.makePlan("agent:untrusted",
                           Json::array({op("midi.clip.trim",
                                           {{"clip", src}, {"start_samples", 108000}, {"end_samples", 336000}})}));
            },
            "external actor cannot grant local boundary editing");
        auto stale = c.makePlan(
            "human",
            Json::array({op("midi.clip.trim", {{"clip", src}, {"start_samples", 108000}, {"end_samples", 336000}})}));
        run(c, Json::array({op("track.gain", {{"track", owner}, {"db", -3.}})}));
        const auto before = c.query();
        rejects([&] { c.commit(stale); }, "stale trim cannot overwrite newer human state");
        check(c.query() == before, "stale trim refusal preserves human state");
        open(fixture);
        const auto onset = renderOnset(c, dir.getChildFile("Before.wav"), 0, 816000);
        const auto result = run(
            c,
            Json::array({op("midi.clip.trim", {{"clip", src}, {"start_samples", 192000}, {"end_samples", 336000}})}));
        const auto trimmed = renderOnset(c, dir.getChildFile("Trimmed.wav"), 0, 816000);
        check(onset == 96000 && trimmed == 192000,
              "actual independently decoded FourOsc onset follows clip visibility not rewritten events");
        c.undo(result["plan_id"]);
        pump();
        check(renderOnset(c, dir.getChildFile("Undo.wav"), 0, 816000) == onset,
              "native Undo restores actual rendered onset");
        check(juce::SHA256(fixture).toHexString() == inputHash, "original native input file bytes untouched");
        Json report = {{"state", "passed"},
                       {"checks", checks},
                       {"geometry_cases", cases},
                       {"fractional_native_case", true},
                       {"event_seconds_budget", 1e-9},
                       {"rounded_48k_samples", "exact"},
                       {"source_SEQ", "unchanged"},
                       {"automation", "unchanged project time and explicit base"},
                       {"render_before_onset", onset},
                       {"render_trimmed_onset", trimmed},
                       {"demo", demo.getFullPathName().toStdString()},
                       {"physical_GUI", "not executed"},
                       {"limits", "loops quantisation Groove expression and time range Nudge not qualified"}};
        if (argc > 1)
            std::ofstream(argv[1]) << report.dump(2) << '\n';
        std::cout << report.dump(2) << std::endl;
        w.setVisible(false);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << std::endl;
        return 1;
    }
}
