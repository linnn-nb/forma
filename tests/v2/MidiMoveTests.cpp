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
        w.selectAudioClip(id);
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
                             .getChildFile("midi-move-" + juce::Uuid().toString());
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
        double worstCurveError = 0;
        for (const auto& file : {fixture, ramp})
            for (const auto& basis : {"beats", "samples"})
            {
                open(file);
                run(c, Json::array({op("midi.clip.timebase.set", {{"clip", src}, {"basis", basis}})}));
                auto initial = timing(c, src);
                auto expected = initial;
                const auto rawHash = treeHash(AudioDeviceTestAccess::clip(c, src).getSequence().state);
                const auto nativeBefore = AudioDeviceTestAccess::clip(c, src).state.createCopy();
                const auto before = actualClip(c, src);
                const auto& tempo = AudioDeviceTestAccess::tempo(c).getInternalSequence();
                const double deltaBeat = tempo.toBeats(tracktion::TimePosition::fromSeconds(9)).inBeats() -
                                         before["start_beat"].get<double>();
                for (auto& e : expected)
                    for (const char* f : {"start", "end"})
                        e[f] = std::string(basis) == "samples"
                                   ? e[f].get<double>() + 7.
                                   : tempo
                                         .toTime(tracktion::BeatPosition::fromBeats(
                                             tempo.toBeats(tracktion::TimePosition::fromSeconds(e[f])).inBeats() +
                                             deltaBeat))
                                         .inSeconds();
                auto predicted = c.midiClipMoveExtent(src, 432000, c.sessionToken(), c.querySummary()["revision"]);
                check(!predicted.contains("sample_midi_projection") && !predicted.contains("state_hash"),
                      "geometry preview performs no event projection and carries no stale checksum");
                auto plan = c.makePlan(
                    "human", Json::array({op("midi.clip.move", {{"clip", src}, {"position_samples", 432000}})}));
                const auto receipt = c.commit(plan);
                pump();
                check(receipt["state"] == "committed" && c.commit(plan)["replayed"] == true,
                      "MIDI move committed and retry is idempotent");
                sameTimes(c, src, expected, 0);
                const auto after = actualClip(c, src);
                check(after["start_samples"] == 432000 && after["length_samples"] == predicted["length_samples"] &&
                          std::abs(AudioDeviceTestAccess::clip(c, src).getPosition().getOffset().inSeconds() -
                                   predicted["source_offset_seconds"].get<double>()) <= 1e-12,
                      "native position source offset and visible extent match preflight");
                if (std::string(basis) == "samples")
                    origin(c, src, rawHash);
                else
                    check(rawHash == treeHash(AudioDeviceTestAccess::clip(c, src).getSequence().state),
                          "beat move preserves raw native SEQ and unknown properties");
                const auto saved = dir.getChildFile("Move-" + juce::String(cases) + ".tracktionedit");
                c.save(saved);
                c.undo(receipt["plan_id"]);
                pump();
                sameTimes(c, src, initial, 0);
                check(AudioDeviceTestAccess::clip(c, src).state.isEquivalentTo(nativeBefore),
                      "single native Undo restores complete actual MIDI clip including provenance");
                c.redo();
                pump();
                sameTimes(c, src, expected, 0);
                open(saved);
                sameTimes(c, src, expected, 0);
                if (std::string(basis) == "samples")
                    origin(c, src, rawHash);
                check(actualClip(c, src)["start_samples"] == 432000, "save close reopen retains actual move");
                ++cases;
            }
        // Move a real shared curve through both native clocks, including non-linear Tempo.
        for (const auto& file : {fixture, ramp})
            for (const auto& clock : {"samples", "beats"})
            {
                open(file);
                run(c, Json::array({op("track.automation_edit_basis.set", {{"track", owner}, {"basis", clock}})}));
                std::string parameter;
                const auto lanes = c.automationQuery(owner)["lanes"];
                for (const auto& lane : lanes)
                    if (lane["parameter"] == "volume")
                        parameter = lane["id"];
                check(!parameter.empty(), "actual native volume lane identified");
                Json points = Json::array();
                for (int i = 0; i < 8; ++i)
                    points.push_back(op("automation.point.add", {{"track", owner},
                                                                 {"parameter", parameter},
                                                                 {"position_samples", i * 48000},
                                                                 {"value", i % 2 ? -6. : -18.},
                                                                 {"curve", i % 2 ? .75 : -.75},
                                                                 {"ref", "$point-" + std::to_string(i)}}));
                run(c, points);
                auto* native = AudioDeviceTestAccess::parameter(c, owner, parameter);
                te::AutomationIterator frozen(*native);
                const double span = native->valueRange.end - native->valueRange.start;
                const auto oldCurves = persistent(c, owner);
                const auto& seq = AudioDeviceTestAccess::tempo(c).getInternalSequence();
                const auto old = actualClip(c, src);
                const double beatDelta =
                    seq.toBeats(tracktion::TimePosition::fromSeconds(9)).inBeats() - old["start_beat"].get<double>();
                std::vector<std::pair<double, double>> probes;
                for (int i = 1; i < 250; ++i)
                {
                    const double time = 2. + i * .02;
                    const double destTime =
                        std::string(clock) == "samples"
                            ? time + 7
                            : seq
                                  .toTime(tracktion::BeatPosition::fromBeats(
                                      seq.toBeats(tracktion::TimePosition::fromSeconds(time)).inBeats() + beatDelta))
                                  .inSeconds();
                    probes.emplace_back(destTime, nativeValue(frozen, time));
                }
                auto moved = run(c, Json::array({op("midi.clip.move", {{"clip", src}, {"position_samples", 432000}})}));
                te::AutomationIterator actual(*AudioDeviceTestAccess::parameter(c, owner, parameter));
                for (const auto& [time, want] : probes)
                {
                    const double got = nativeValue(actual, time);
                    const double error = std::abs(want - got);
                    worstCurveError = std::max(worstCurveError, error / span);
                    const double ulp = std::abs(double(std::nextafter(float(want), INFINITY)) - float(want));
                    check(error <= span * 1e-7 + 2 * ulp,
                          "actual native shared curve obeys declared span budget plus two float ULP");
                }
                const auto movedCurves = persistent(c, owner);
                for (const auto& lane : oldCurves)
                    for (const auto& point : lane["points"])
                        if (point["position_samples"] >= 96000 && point["position_samples"] < 336000)
                        {
                            const double time = point["position_samples"].get<double>() / 48000.;
                            const double destination =
                                std::string(clock) == "samples"
                                    ? time + 7
                                    : seq.toTime(tracktion::BeatPosition::fromBeats(
                                                     seq.toBeats(tracktion::TimePosition::fromSeconds(time)).inBeats() +
                                                     beatDelta))
                                          .inSeconds();
                            bool found = false;
                            for (const auto& movedLane : movedCurves)
                                for (const auto& movedPoint : movedLane["points"])
                                    found |= movedPoint["id"] == point["id"] &&
                                             movedPoint["position_samples"] == std::llround(destination * 48000.);
                            check(found, "real transported native curve point ID and position retained");
                        }
                auto saved = dir.getChildFile("Curves-" + juce::String(cases) + ".tracktionedit");
                c.save(saved);
                c.undo(moved["plan_id"]);
                pump();
                check(persistent(c, owner) == oldCurves, "MIDI and shared curve use one native Undo");
                c.redo();
                pump();
                check(persistent(c, owner) == movedCurves, "Redo restores exact transported curve IDs");
                open(saved);
                check(persistent(c, owner) == movedCurves, "shared native curve retains exact state after reopen");
                ++cases;
            }
        // Mixed Edit groups move actual audio and MIDI in either anchor direction.
        open(fixture);
        const auto wave = dir.getChildFile("Peer.wav");
        {
            juce::WavAudioFormat format;
            auto stream = std::make_unique<juce::FileOutputStream>(wave);
            std::unique_ptr<juce::AudioFormatWriter> writer(
                format.createWriterFor(stream.release(), 48000, 2, 24, {}, 0));
            juce::AudioBuffer<float> pcm(2, 240000);
            for (int i = 0; i < pcm.getNumSamples(); ++i)
                for (int ch = 0; ch < 2; ++ch)
                    pcm.setSample(ch, i, .05f * std::sin(float(i) * .04f));
            check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()),
                  "owned actual audio group media written");
        }
        run(c, Json::array({op("track.create", {{"name", "Audio peer"}, {"type", "audio"}, {"ref", "$peer"}}),
                            op("clip.import", {{"track", "$peer"},
                                               {"path", wave.getFullPathName().toStdString()},
                                               {"position_samples", 96000},
                                               {"ref", "$peerclip"}})}));
        query = c.query();
        const std::string peer = query["tracks"][2]["id"], peerClip = query["tracks"][2]["clips"][0]["id"];
        run(c, Json::array({op("group.create", {{"id", "move-group"},
                                                {"name", "Audio + MIDI"},
                                                {"members", Json::array({owner, peer})},
                                                {"enabled", true},
                                                {"edit", true},
                                                {"mute", false},
                                                {"solo", false}})}));
        const auto groupFile = dir.getChildFile("Group.tracktionedit");
        c.save(groupFile);
        const auto waveHash = juce::SHA256(wave).toHexString();
        for (bool midiAnchor : {true, false})
        {
            open(groupFile);
            const auto receipt =
                run(c, Json::array({op(midiAnchor ? "midi.clip.move" : "clip.move",
                                       {{"clip", midiAnchor ? src : peerClip}, {"position_samples", 432000}})}));
            check(actualClip(c, src)["start_samples"] == 432000 && actualClip(c, peerClip)["start_samples"] == 432000,
                  "both audio and MIDI group anchors expand to one real mixed move");
            c.undo(receipt["plan_id"]);
            pump();
            check(actualClip(c, src)["start_samples"] == 96000 && actualClip(c, peerClip)["start_samples"] == 96000,
                  "mixed group Undo restores both stable clips");
        }
        check(juce::SHA256(wave).toHexString() == waveHash, "original group PCM unchanged");
        open(fixture);
        const auto stale =
            c.makePlan("human", Json::array({op("midi.clip.move", {{"clip", src}, {"position_samples", 432000}})}));
        run(c, Json::array({op("track.gain", {{"track", owner}, {"db", -3.}})}));
        const auto guarded = c.query();
        rejects([&] { c.commit(stale); }, "stale move cannot overwrite newer human edit");
        check(c.query() == guarded, "stale refusal changes no native state or revision");
        rejects(
            [&]
            { c.makePlan("human", Json::array({op("midi.clip.move", {{"clip", src}, {"position_samples", -1}})})); },
            "move before session start refused");
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
            check(bad->writeTo(file), "owned lock or unqualified MIDI fixture saved");
            open(file);
            const auto before = c.query();
            rejects(
                [&]
                {
                    c.makePlan("human",
                               Json::array({op("midi.clip.move", {{"clip", src}, {"position_samples", 432000}})}));
                },
                "locked looping or expressive MIDI move refused before writes");
            check(c.query() == before, "unsupported MIDI move refusal changes no state or revision");
        }
        open(fixture);
        const auto guardedNative = c.query();
        rejects(
            [&]
            {
                c.makePlan("human",
                           Json::array({op("midi.clip.move",
                                           {{"clip", src}, {"position_samples", 432000}, {"state_hash", "forged"}})}));
            },
            "forged MIDI source hash is rejected instead of being silently replaced");
        rejects(
            [&]
            {
                c.makePlan("agent:untrusted",
                           Json::array({op("midi.clip.move", {{"clip", src}, {"position_samples", 432000}})}));
            },
            "local MIDI move cannot be granted by an external actor string");
        check(c.query() == guardedNative, "source hash and actor refusals preserve state and revision");
        // Production shared Nudge keys, actual source basis, and independently decoded instrument output.
        open(fixture);
        run(c, Json::array({op("midi.clip.timebase.set", {{"clip", src}, {"basis", "samples"}})}));
        for (const auto& unit : {"sample", "10ms", "100ms", "beat", "quarter-beat"})
        {
            c.updateUiState({{"nudge", unit}, {"edit_mode", "slip"}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            AudioDeviceTestAccess::select(w, src);
            pump();
            const auto before = timing(c, src);
            const int64_t delta = std::string(unit) == "sample" ? 1
                                  : std::string(unit) == "10ms" ? 480
                                  : std::string(unit) == "100ms"
                                      ? 4800
                                      : c.offsetByBeats(96000, std::string(unit) == "beat" ? 1 : .25) - 96000;
            check(w.uiCommands().invokeDirectly(editCommand::nudgeForward, false),
                  "production MIDI Nudge command enabled");
            pump();
            sameTimes(c, src, before, delta / 48000.);
            check(actualClip(c, src)["start_samples"] == 96000 + delta,
                  "whole MIDI Nudge moves native clip and all events");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            sameTimes(c, src, before, 0);
        }
        {
            auto& area = AudioDeviceTestAccess::area(w);
            c.updateUiState({{"start_samples", 0},
                             {"span_samples", int64_t(area.coordinates().width) * 480},
                             {"edit_tool", "grabber"},
                             {"edit_mode", "slip"}},
                            c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            pump();
            const auto rect = area.clipRect(actualClip(c, src), 0);
            const float displacement = float(area.coordinates().pixelAt(120000) - area.coordinates().pixelAt(96000));
            const int y = rect.getCentreY();
            const auto before = timing(c, src);
            area.mouseDown(event(area, float(rect.getCentreX()), y));
            area.mouseDrag(event(area, float(rect.getCentreX()) + displacement, y, true));
            juce::Image image(juce::Image::ARGB, area.getWidth(), area.getHeight(), true);
            juce::Graphics graphics(image);
            area.paint(graphics); // Real draft must preserve stable IDs and mapped visible notes.
            area.mouseUp(event(area, float(rect.getCentreX()) + displacement, y, true));
            pump();
            check(actualClip(c, src)["start_samples"] == 120000, "production Grabber moves actual MIDI clip");
            sameTimes(c, src, before, .5);
            w.uiCommands().invokeDirectly(6, false);
            pump();
            sameTimes(c, src, before, 0);
            AudioDeviceTestAccess::select(w, src);
            pump();
            check(w.uiCommands().invokeDirectly(editCommand::spot, false), "production MIDI Spot command opens panel");
            pump();
            auto* bar = dynamic_cast<juce::TextEditor*>(find(w, "edit.spot.bar"));
            auto* beat = dynamic_cast<juce::TextEditor*>(find(w, "edit.spot.beat"));
            if (!(bar && beat && bar->isVisible() && bar->getParentComponent()->isVisible()))
                std::cerr << "Spot context " << AudioDeviceTestAccess::context(w).dump() << std::endl;
            check(bar && beat && bar->isVisible() && bar->getParentComponent()->isVisible(),
                  "actual musical Spot component configured visible for MIDI");
            bar->setText("3");
            beat->setText("1");
            const auto target = c.sampleAtBarBeat(3, 1);
            click(w, "edit.spot.apply");
            check(actualClip(c, src)["start_samples"] == target,
                  "production MIDI Spot applies actual musical location");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            sameTimes(c, src, before, 0);
        }
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom(
            'j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        keys->clearAllKeyPresses(editCommand::nudgeForward);
        keys->addKeyPress(editCommand::nudgeForward, custom);
        pump();
        c.updateUiState({{"nudge", "10ms"}}, c.sessionToken());
        AudioDeviceTestAccess::refresh(w);
        AudioDeviceTestAccess::select(w, src);
        pump();
        check(w.keyPressed(custom), "custom whole MIDI Nudge key invokes production command");
        pump();
        check(actualClip(c, src)["start_samples"] == 96480, "custom key has real native result");
        auto demo = dir.getChildFile("MidiMoveReady.tracktionedit");
        c.save(demo);
        open(demo);
        AudioDeviceTestAccess::select(w, src);
        pump();
        check(w.keyPressed(custom), "custom whole MIDI key retained after reopen");
        pump();
        check(actualClip(c, src)["start_samples"] == 96960, "first post-open shortcut performs actual move");
        open(fixture);
        const auto initialOnset = renderOnset(c, dir.getChildFile("Before.wav"), 0, 816000);
        const auto receipt = run(c, Json::array({op("midi.clip.move", {{"clip", src}, {"position_samples", 432000}})}));
        const auto movedOnset = renderOnset(c, dir.getChildFile("After.wav"), 0, 816000);
        check(movedOnset > initialOnset && initialOnset >= 96000 && movedOnset >= 432000,
              "independent FourOsc PCM onset moves to real destination");
        c.undo(receipt["plan_id"]);
        pump();
        check(renderOnset(c, dir.getChildFile("Undo.wav"), 0, 816000) == initialOnset,
              "native audio render returns to original onset after Undo");
        check(juce::SHA256(fixture).toHexString() == inputHash, "original native fixture bytes unchanged");
        Json report = {{"state", "passed"},
                       {"checks", checks},
                       {"clock_cases", cases},
                       {"worst_curve_error_parameter_span", worstCurveError},
                       {"event_time_tolerance_seconds", 1e-9},
                       {"rounded_48k_samples", "exact"},
                       {"curve_tolerance_parameter_span", 1e-7},
                       {"additional_curve_float_ulp", 2},
                       {"demo", demo.getFullPathName().toStdString()},
                       {"render_before_onset", initialOnset},
                       {"render_after_onset", movedOnset},
                       {"physical_gui", "not executed"},
                       {"limits", "no loop quantisation Groove expression or MIDI trim qualification"}};
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
