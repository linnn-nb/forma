#include "RecoveryStore.h"
#include "ui/Workspace.h"
#include <fstream>
#include <iostream>
#include <iomanip>
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
    static std::unique_ptr<te::AutomationCurve> freeze(Commands& c, const std::string& track,
                                                       const std::string& parameter)
    {
        auto state = c.automationParameter(track, parameter)->getCurve().state.createCopy();
        juce::ValueTree parent("QUALIFICATION");
        parent.addChild(state, -1, nullptr);
        return std::make_unique<te::AutomationCurve>(*c.edit, te::AutomationCurve::TimeBase::time, parent, state);
    }
    static float value(te::AutomationCurve& curve, double seconds)
    {
        te::AutomationIterator iterator(curve.edit, curve);
        iterator.setPosition(tracktion::TimePosition::fromSeconds(seconds));
        return iterator.getCurrentValue();
    }
    static float value(Commands& c, const std::string& track, const std::string& parameter, double seconds)
    {
        te::AutomationIterator iterator(*c.automationParameter(track, parameter));
        iterator.setPosition(tracktion::TimePosition::fromSeconds(seconds));
        return iterator.getCurrentValue();
    }
    static double span(Commands& c, const std::string& track, const std::string& parameter)
    {
        auto* p = c.automationParameter(track, parameter);
        return p->valueRange.end - p->valueRange.start;
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
    static auto snapshot(Commands& c)
    {
        return c.recoverySnapshot();
    }
    static void restore(Commands& c, juce::ValueTree state)
    {
        c.restoreRecoveryState(std::move(state), c.sessionToken(), c.querySummary()["revision"]);
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
    explicit Storage(juce::File f) : PropertyStorage("Forma object Shuffle qualification"), folder(f) {}
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
        t["pan"] = t["base_pan"];
        auto lanes = c.automationQuery(t["id"])["lanes"];
        // The native playback readout is derived, not the reversible explicit base.
        // Keep all explicit values, curve points and static parameter values exact.
        for (auto& plugin : t["plugins"])
            for (auto& parameter : plugin["parameters"])
                for (const auto& lane : lanes)
                    if (lane["owner"] == plugin["id"] && lane["parameter"] == parameter["id"] &&
                        !lane["points"].empty())
                    {
                        parameter.erase("current_value");
                        parameter.erase("display");
                    }
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
    const auto dir = parent.getChildFile("object-shuffle-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1440, 900);
        auto& c = AudioDeviceTestAccess::owner(w);
        std::vector<juce::File> media;
        std::vector<std::string> hashes;
        for (const double length : {2., 1., .5})
        {
            const auto file = dir.getChildFile("Known-" + juce::String(length) + ".wav");
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
            auto writer = wav.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
            check(bool(writer), "owned actual stereo PCM writer created");
            juce::AudioBuffer<float> signal(2, int(length * 44100));
            for (int i = 0; i < signal.getNumSamples(); ++i)
            {
                signal.setSample(0, i, .08f * std::sin(float(i) * .047f));
                signal.setSample(1, i, .065f * std::cos(float(i) * .071f));
            }
            check(writer->writeFromAudioSampleBuffer(signal, 0, signal.getNumSamples()), "real known media written");
            writer.reset();
            media.push_back(file);
            hashes.push_back(Commands::mediaHash(file));
        }
        Json ops = Json::array(
            {operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}}),
             operation("tempo.event.create", {{"beat_position", 16.}, {"bpm", 180.}}),
             operation("tempo.event.create", {{"beat_position", 32.}, {"bpm", 80.}}),
             operation("meter.event.create", {{"beat_position", 20.}, {"numerator", 3}, {"denominator", 8}}),
             operation("meter.event.create", {{"beat_position", 32.}, {"numerator", 4}, {"denominator", 4}})});
        for (int i = 0; i < 5; ++i)
            ops.push_back(operation("track.create", {{"ref", "$t" + std::to_string(i)},
                                                     {"name", i < 2   ? "Source " + std::to_string(i + 1)
                                                              : i < 4 ? "Destination " + std::to_string(i - 1)
                                                                      : "Unselected reference"},
                                                     {"type", i < 4 ? "instrument" : "audio"}}));
        auto import = [&](int track, double at, int file)
        {
            return operation("clip.import", {{"track", "$t" + std::to_string(track)},
                                             {"path", media[file].getFullPathName().toStdString()},
                                             {"media_hash", hashes[file]},
                                             {"position_samples", std::llround(at * 48000)}});
        };
        for (int i = 0; i < 4; ++i)
            ops.push_back(operation("plugin.insert", {{"track", "$t" + std::to_string(i)}, {"type", "4bandEq"}}));
        for (const auto& v : std::vector<std::tuple<int, double, int>>{{0, 1, 0},
                                                                       {0, 4, 2},
                                                                       {0, 16, 0},
                                                                       {1, 1.5, 0},
                                                                       {1, 4.5, 2},
                                                                       {1, 6, 1},
                                                                       {1, 16, 0},
                                                                       {2, 8, 0},
                                                                       {2, 20, 0},
                                                                       {3, 8, 0},
                                                                       {3, 20, 0},
                                                                       {4, 0, 0}})
            ops.push_back(import(std::get<0>(v), std::get<1>(v), std::get<2>(v)));
        int n = 0;
        for (const auto& v : std::vector<std::tuple<int, double, double>>{{0, 5, 1},
                                                                          {0, 3.5, .5},
                                                                          {0, 16, 2},
                                                                          {1, 2, 1},
                                                                          {1, 16, 2},
                                                                          {2, 8, 4},
                                                                          {2, 20, 2},
                                                                          {3, 8, 4},
                                                                          {3, 20, 2}})
        {
            const auto ref = "$m" + std::to_string(n++);
            ops.push_back(operation("midi.clip.create", {{"track", "$t" + std::to_string(std::get<0>(v))},
                                                         {"ref", ref},
                                                         {"name", "Native MIDI " + ref},
                                                         {"position_samples", std::llround(std::get<1>(v) * 48000)},
                                                         {"length_samples", std::llround(std::get<2>(v) * 48000)}}));
            ops.push_back(operation("midi.note.add", {{"clip", ref},
                                                      {"position_samples", std::llround(std::get<1>(v) * 48000)},
                                                      {"length_samples", std::llround(std::get<2>(v) * 48000)},
                                                      {"pitch", 60 + n},
                                                      {"velocity", 90}}));
        }
        run(c, ops);
        auto query = c.query();
        std::vector<std::string> owners;
        for (const auto& t : query["tracks"])
            owners.push_back(t["id"]);
        auto find = [&](int owner, const std::string& kind, double at)
        {
            const auto q = c.query();
            for (const auto& t : q["tracks"])
                if (t["id"] == owners[owner])
                    for (const auto& v : t["clips"])
                        if (v["kind"] == kind && v["start_samples"] == std::llround(at * 48000))
                            return v["id"].get<std::string>();
            throw std::runtime_error("actual fixture clip missing");
        };
        const Json selected = Json::array(
            {find(0, "audio", 1), find(0, "midi", 5), find(1, "audio", 1.5), find(1, "midi", 2), find(1, "audio", 6)});
        const Json originalMidi = Json::array({find(0, "midi", 3.5), find(0, "midi", 16), find(1, "midi", 16)});
        const auto suffixAudio = find(0, "audio", 16), suffixMidi = find(0, "midi", 16);
        std::vector<std::vector<std::string>> parameters;
        std::vector<std::string> synths;
        for (int i = 0; i < 4; ++i)
        {
            std::string eq, synth;
            for (const auto& p : query["tracks"][i]["plugins"])
            {
                if (p["type"] == "4osc")
                    synth = p["id"];
                for (const auto& a : p["parameters"])
                    if (a["id"] == "Mid gain 1")
                        eq = p["id"].get<std::string>() + "::Mid gain 1";
            }
            check(!eq.empty() && !synth.empty(), "actual EQ and instrument parameters enumerated");
            synths.push_back(synth);
            parameters.push_back({"volume", "pan", eq});
            run(c, Json::array({operation("plugin.bypass", {{"plugin", synth}, {"bypassed", true}})}));
            Json points = Json::array();
            for (size_t p = 0; p < 3; ++p)
            {
                const std::vector<double> values = p == 0   ? std::vector<double>{-24, -18, -6, -12, -20}
                                                   : p == 1 ? std::vector<double>{-.5, .2, .6, -.3, -.4}
                                                            : std::vector<double>{-3, 3, -2, 2, -1};
                int index = 0;
                for (const int64_t at :
                     {int64_t{0}, int64_t{2 * 48000}, int64_t{3 * 48000}, int64_t{15 * 48000}, int64_t{24 * 48000}})
                {
                    points.push_back(operation("automation.point.add",
                                               {{"track", owners[i]},
                                                {"parameter", parameters[i][p]},
                                                {"position_samples", at},
                                                {"value", values[index]},
                                                {"curve", (index % 2 ? 1. : -1.) * (i < 2 ? .25 : .75)},
                                                {"ref", "$p" + std::to_string(p) + "-" + std::to_string(index)}}));
                    ++index;
                }
            }
            run(c, points);
        }
        const auto base = dir.getChildFile("Base.tracktionedit");
        c.save(base);
        auto xml = juce::XmlDocument::parse(base);
        for (const auto& owner : query["tracks"])
            for (const auto& v : owner["clips"])
                if (v["kind"] == "midi")
                {
                    auto* node = byID(*xml, v["id"]);
                    node->setAttribute("qualification_extra", "raw object Shuffle MIDI");
                    auto* seq = node->getChildByName("SEQUENCE");
                    seq->addChildElement(te::MidiControllerEvent::createControllerEvent(
                                             tracktion::BeatPosition::fromBeats(.25), 1, 96 << 7, 0)
                                             .createXml()
                                             .release());
                    const uint8_t bytes[]{0x7d, 0x32};
                    seq->addChildElement(
                        te::MidiSysexEvent::createSysexEvent(juce::MidiMessage::createSysExMessage(bytes, 2),
                                                             tracktion::BeatPosition::fromBeats(.5))
                            .createXml()
                            .release());
                }
        const auto step = dir.getChildFile("Step.tracktionedit");
        check(xml->writeTo(step), "actual raw native fixture written");
        const auto stepHash = juce::SHA256(step).toHexString();
        auto rampXml = juce::XmlDocument::parse(step);
        for (auto* event : rampXml->getChildByName("TEMPOSEQUENCE")->getChildIterator())
            if (event->hasTagName("TEMPO") &&
                (event->getDoubleAttribute("startBeat") == 8 || event->getDoubleAttribute("startBeat") == 16))
                event->setAttribute("curve", event->getDoubleAttribute("startBeat") == 8 ? .35 : -.25);
        const auto ramp = dir.getChildFile("Ramp.tracktionedit");
        check(rampXml->writeTo(ramp), "native ramp fixture written");
        auto constantXml = juce::XmlDocument::parse(step);
        auto* tempo = constantXml->getChildByName("TEMPOSEQUENCE");
        for (auto* event = tempo->getFirstChildElement(); event;)
        {
            auto* next = event->getNextElement();
            if (event->getDoubleAttribute("startBeat") != 0)
                tempo->removeChildElement(event, true);
            event = next;
        }
        const auto constant = dir.getChildFile("Constant.tracktionedit");
        check(constantXml->writeTo(constant), "constant native Tempo Meter fixture written");
        const std::vector<std::pair<juce::File, std::string>> profiles{
            {step, "native"}, {ramp, "native"}, {constant, "samples"}, {constant, "native"}};
        auto open = [&](const juce::File& file)
        {
            w.openSession(file);
            pump();
        };
        auto setBasis = [&](const std::string& basis)
        {
            for (int i = 0; i < 4; ++i)
                run(c, Json::array(
                           {operation("track.automation_edit_basis.set", {{"track", owners[i]}, {"basis", basis}})}));
        };
        auto copyObjects = [&]
        {
            auto b = c.prepareTimelineClipClipboard(selected, c.sessionToken(), c.querySummary()["revision"]);
            c.acceptClipboard(b["id"]);
            check(!b.value("source_range", false), "actual whole-object clipboard retains object identity");
            return b;
        };
        auto beatAt = [&](double at)
        { return AudioDeviceTestAccess::sequence(c).toBeats(tracktion::TimePosition::fromSeconds(at)).inBeats(); };
        auto timeAt = [&](double at)
        { return AudioDeviceTestAccess::sequence(c).toTime(tracktion::BeatPosition::fromBeats(at)).inSeconds(); };
        size_t probes = 0, clearCases = 0, pasteCases = 0, recoveryDoubles = 0;
        double maximumError = 0;
        auto compare = [&](int owner, size_t parameter, double at, float expected)
        {
            const double span = AudioDeviceTestAccess::span(c, owners[owner], parameters[owner][parameter]);
            const double error = std::abs(
                double(AudioDeviceTestAccess::value(c, owners[owner], parameters[owner][parameter], at)) - expected);
            const double ulp =
                std::abs(double(std::nextafter(expected, std::numeric_limits<float>::infinity())) - expected);
            ++probes;
            maximumError = std::max(maximumError, error / span);
            if (error > 1e-7 * span + 2 * ulp)
                std::cerr << "DSP_DIFF expected " << std::setprecision(17) << expected << " actual "
                          << AudioDeviceTestAccess::value(c, owners[owner], parameters[owner][parameter], at)
                          << " span " << span << std::endl;
            if (error > 1e-7 * span + 2 * ulp)
                throw std::runtime_error("object Shuffle exceeds predetermined native DSP span budget at " +
                                         std::to_string(at) + " owner " + std::to_string(owner) + " parameter " +
                                         std::to_string(parameter) + " error " + std::to_string(error));
        };
        for (const auto& [file, mapping] : profiles)
            for (const std::string basis : {"samples", "beats"})
                for (const std::string action : {"cut", "delete"})
                {
                    open(file);
                    setBasis(basis);
                    const auto b = copyObjects();
                    const auto before = persistentTracks(c);
                    std::vector<std::vector<std::unique_ptr<te::AutomationCurve>>> originals(2);
                    for (int owner = 0; owner < 2; ++owner)
                        for (const auto& p : parameters[owner])
                            originals[owner].push_back(AudioDeviceTestAccess::freeze(c, owners[owner], p));
                    auto plan = c.makePlan(
                        "human", Json::array({operation("timeline.clips.erase", {{"clipboard", b["id"]},
                                                                                 {"action", action},
                                                                                 {"ripple", true},
                                                                                 {"ripple_mapping", mapping}})}));
                    const auto impact = c.preview(plan)["timeline_changes"][0];
                    std::vector<float> expectedAfterOpen;
                    check(impact["selection_kind"] == "objects" && impact["range"].is_null() &&
                              impact["object_intervals"].size() == 2 && impact["action"] == action,
                          "sealed preview declares actual per-track occupied interval unions");
                    check(impact["object_intervals"][0]["intervals"].size() == 2 &&
                              impact["object_intervals"][1]["intervals"].size() == 2,
                          "overlapping selections merge while separate intervals preserve gaps");
                    Scope restricted;
                    restricted.targets = {owners[0], owners[1]};
                    restricted.end = 8 * 48000;
                    rejects([&] { c.review(plan, restricted); },
                            "object Shuffle time Scope includes all later clips and shared curves");
                    std::map<std::string, Json> originalsMidi;
                    for (const auto& id : originalMidi)
                        originalsMidi[id.get<std::string>()] = nativeData(file, id);
                    auto receipt = c.commit(plan);
                    pump();
                    check(c.commit(plan)["replayed"] == true, "object Shuffle retry is idempotent");
                    for (const auto& id : selected)
                    {
                        bool exists = false;
                        const auto q = c.query();
                        for (const auto& t : q["tracks"])
                            for (const auto& v : t["clips"])
                                exists |= v["id"] == id;
                        check(!exists, "only explicitly selected whole objects removed");
                    }
                    for (int owner = 0; owner < 2; ++owner)
                    {
                        const auto& intervals = impact["object_intervals"][owner]["intervals"];
                        for (const auto& v : before[owner]["clips"])
                        {
                            if (std::find(selected.begin(), selected.end(), v["id"]) != selected.end())
                                continue;
                            double removed = 0, removedBeats = 0;
                            for (const auto& interval : intervals)
                                if (interval["end_samples"].get<int64_t>() <= v["start_samples"].get<int64_t>())
                                {
                                    removed += (interval["end_samples"].get<int64_t>() -
                                                interval["start_samples"].get<int64_t>()) /
                                               48000.;
                                    removedBeats += interval["removed_beats"].get<double>();
                                }
                            const auto moved = clip(c, v["id"]);
                            if (mapping == "native" && v["kind"] == "midi" && v["timebase"] == "beats")
                                check(
                                    std::abs(moved["start_beat"].get<double>() -
                                             (v["start_beat"].get<double>() - removedBeats)) <= 1e-10 &&
                                        std::abs(moved["length_beats"].get<double>() -
                                                 v["length_beats"].get<double>()) <= 1e-10,
                                    "remaining native MIDI retains beat duration and accumulated musical displacement");
                            else
                                check(moved["start_samples"].get<int64_t>() ==
                                              v["start_samples"].get<int64_t>() - std::llround(removed * 48000) &&
                                          moved["length_samples"] == v["length_samples"],
                                      "audio or constant-tempo suffix retains length and only closes prior selected "
                                      "intervals");
                        }
                        const bool musical = mapping == "native" && basis == "beats";
                        for (size_t p = 0; p < 3; ++p)
                            for (int i = 0; i < 512; ++i)
                            {
                                const double at = std::llround(26. * i / 511. * 48000) / 48000.;
                                double original = musical ? beatAt(at) : at;
                                for (const auto& interval : intervals)
                                {
                                    const double first = interval["start_samples"].get<int64_t>() / 48000.,
                                                 last = interval["end_samples"].get<int64_t>() / 48000.;
                                    if (original >= (musical ? beatAt(first) : first))
                                        original += musical ? beatAt(last) - beatAt(first) : last - first;
                                }
                                const float expected = AudioDeviceTestAccess::value(
                                    *originals[owner][p], musical ? timeAt(original) : original);
                                expectedAfterOpen.push_back(expected);
                                compare(owner, p, at, expected);
                            }
                    }
                    check(
                        true,
                        "all actual volume pan EQ curve values follow composed interval collapse within fixed budget");
                    originals.clear();
                    const auto after = persistentTracks(c);
                    check(after[4] == before[4], "unselected reference audio and state stay exact");
                    const auto saved = dir.getChildFile("Clear-" + juce::String(clearCases++) + ".tracktionedit");
                    c.save(saved);
                    for (const auto& [id, data] : originalsMidi)
                        check(nativeData(saved, id) == data, "retained NOTE CONTROL SYSEX and opaque fields unchanged");
                    c.undo(receipt["plan_id"]);
                    pump();
                    check(persistentTracks(c) == before, "whole-object clear is one native Undo after Save");
                    c.redo();
                    pump();
                    check(persistentTracks(c) == after, "whole-object clear Redo exact");
                    open(saved);
                    const auto restored = persistentTracks(c);
                    if (!sameSavedState(restored, after))
                        std::cerr << "OPEN_DIFF " << Json::diff(after, restored).dump() << std::endl;
                    check(sameSavedState(restored, after), "whole-object clear saved native state restored");
                    size_t sampleIndex = 0;
                    for (int owner = 0; owner < 2; ++owner)
                        for (size_t p = 0; p < 3; ++p)
                            for (int i = 0; i < 512; ++i)
                                compare(owner, p, std::llround(26. * i / 511. * 48000) / 48000.,
                                        expectedAfterOpen.at(sampleIndex++));
                    check(
                        true,
                        "reopened native volume pan EQ curves retain their actual DSP values within unchanged budget");
                    if (file == step && basis == "samples" && action == "cut")
                    {
                        const auto beforeRecovery = persistentTracks(c);
                        auto detached = AudioDeviceTestAccess::snapshot(c);
                        const auto exactRecoveryState = detached.first.createCopy();
                        const auto recoveryFolder = dir.getChildFile("PreciseRecovery");
                        auto snapshot =
                            recovery::write(recoveryFolder, {std::move(detached.first), std::move(detached.second)});
                        auto loaded = recovery::read(recoveryFolder, snapshot["id"], snapshot["sha256"]);
                        auto compareDoubles = [&](auto&& self, const juce::ValueTree& original,
                                                  const juce::ValueTree& restored) -> void
                        {
                            check(original.getType() == restored.getType() &&
                                      original.getNumChildren() == restored.getNumChildren(),
                                  "real recovery XML preserves native tree structure");
                            for (int i = 0; i < original.getNumProperties(); ++i)
                            {
                                const auto key = original.getPropertyName(i);
                                const auto value = original[key];
                                if (value.isDouble() && std::isfinite(double(value)))
                                {
                                    ++recoveryDoubles;
                                    check(double(value) == double(restored[key]),
                                          "finite native double round-trips exactly through real recovery XML");
                                }
                            }
                            for (int i = 0; i < original.getNumChildren(); ++i)
                                self(self, original.getChild(i), restored.getChild(i));
                        };
                        compareDoubles(compareDoubles, exactRecoveryState, loaded.state);
                        check(recoveryDoubles > 0, "real snapshot contains finite native doubles");
                        AudioDeviceTestAccess::restore(c, std::move(loaded.state));
                        pump();
                        check(sameSavedState(persistentTracks(c), beforeRecovery),
                              "real recovery write checksum read and native restore retain fractional point positions");
                        run(c, Json::array({operation("track.solo", {{"track", owners[0]}, {"enabled", true}})}));
                        const auto a = clip(c, suffixAudio);
                        const auto begin = a["start_samples"].get<int64_t>();
                        auto pcm = render(c, dir.getChildFile("ActualCollapsedAudio.wav"), begin,
                                          begin + a["length_samples"].get<int64_t>());
                        check(pcm.getMagnitude(0, pcm.getNumSamples()) > .001,
                              "actual moved audio renders through real native volume pan and EQ");
                        run(c, Json::array({operation("plugin.bypass", {{"plugin", synths[0]}, {"bypassed", false}})}));
                        const auto m = clip(c, suffixMidi);
                        const auto start = m["start_samples"].get<int64_t>();
                        auto midiPcm = render(c, dir.getChildFile("ActualCollapsedMIDI.wav"), start,
                                              start + m["length_samples"].get<int64_t>());
                        check(midiPcm.getMagnitude(0, midiPcm.getNumSamples()) > .001,
                              "actual retained MIDI independently renders through native FourOsc");
                    }
                }
        // Object Paste inserts its full copied envelope, including gaps, with native clip clocks.
        for (const auto& [file, mapping] : profiles)
            for (const std::string basis : {"samples", "beats"})
                for (const int64_t removal : {int64_t{0}, int64_t{24000}, int64_t{8 * 48000}})
                {
                    open(file);
                    setBasis(basis);
                    const auto b = copyObjects();
                    const auto before = persistentTracks(c);
                    std::vector<std::vector<std::unique_ptr<te::AutomationCurve>>> sourceCurves(2),
                        destinationCurves(2);
                    for (int owner = 0; owner < 2; ++owner)
                        for (size_t p = 0; p < 3; ++p)
                        {
                            sourceCurves[owner].push_back(
                                AudioDeviceTestAccess::freeze(c, owners[owner], parameters[owner][p]));
                            destinationCurves[owner].push_back(
                                AudioDeviceTestAccess::freeze(c, owners[owner + 2], parameters[owner + 2][p]));
                        }
                    const auto range = c.timelineClipPasteRange(b["id"], 9 * 48000);
                    const double end = range["end_samples"].get<int64_t>() / 48000., right = 9 + removal / 48000.;
                    const auto extent = c.timelineClipPasteExtent(b["id"], 9 * 48000);
                    auto plan = c.makePlan(
                        "human",
                        Json::array({operation("timeline.clips.paste", {{"clipboard", b["id"]},
                                                                        {"tracks", Json::array({owners[2], owners[3]})},
                                                                        {"position_samples", 9 * 48000},
                                                                        {"removal_end_samples", 9 * 48000 + removal},
                                                                        {"mode", "shuffle"},
                                                                        {"ripple_mapping", mapping}})}));
                    const auto preview = c.preview(plan)["timeline_changes"][0];
                    std::cout << "PASTE_CASE " << file.getFileName().toStdString() << " " << mapping << " " << basis
                              << " removal " << removal << " origin " << b["start_samples"] << " sourceEnd "
                              << b["end_samples"] << " end " << std::setprecision(17) << end << " right " << right
                              << std::endl;
                    check(preview["selection_kind"] == "objects" && preview["range"] == range &&
                              preview["displacement_samples"] == std::llround((end - right) * 48000),
                          "object Paste preview reports full envelope and actual replacement displacement");
                    const auto receipt = c.commit(plan);
                    pump();
                    check(pasted(receipt).size() == selected.size(), "all actual selected object snapshots inserted");
                    for (const auto& id : pasted(receipt))
                    {
                        const auto pastedClip = clip(c, id);
                        bool matched = false;
                        for (const auto& e : extent)
                            matched |= pastedClip["kind"] == e["kind"] &&
                                       pastedClip["start_samples"] == e["start_samples"] &&
                                       pastedClip["length_samples"] == e["length_samples"];
                        if (!matched)
                            std::cerr << "EXTENT_DIFF actual " << pastedClip.dump() << " expected " << extent.dump()
                                      << std::endl;
                        check(matched, "inserted whole clip retains its mapped sample or musical placement");
                    }
                    std::vector<float> savedPasteValues;
                    const double origin = b["start_samples"].get<int64_t>() / 48000.,
                                 sourceEnd = b["end_samples"].get<int64_t>() / 48000.;
                    for (int owner = 0; owner < 2; ++owner)
                        for (size_t p = 0; p < 3; ++p)
                            for (int i = 0; i < 512; ++i)
                            {
                                const double at = std::llround(30. * i / 511. * 48000) / 48000.;
                                float expected;
                                if (at < 9)
                                    expected = AudioDeviceTestAccess::value(*destinationCurves[owner][p], at);
                                else if (at >= end)
                                {
                                    const double original = mapping == "native" && basis == "beats"
                                                                ? timeAt(beatAt(at) - (beatAt(end) - beatAt(right)))
                                                                : at - (end - right);
                                    expected = AudioDeviceTestAccess::value(*destinationCurves[owner][p], original);
                                }
                                else
                                {
                                    const double contentEnd = basis == "beats"
                                                                  ? timeAt(beatAt(9) + b["end_beat"].get<double>() -
                                                                           b["start_beat"].get<double>())
                                                                  : 9 + sourceEnd - origin;
                                    // The established half-open join anchors one DESTINATION sample
                                    // before contentEnd, then holds that value to the common end.
                                    const double copiedAt = std::min(at, contentEnd - 1. / 48000.);
                                    const double original = basis == "beats"
                                                                ? timeAt(beatAt(origin) + beatAt(copiedAt) - beatAt(9))
                                                                : origin + copiedAt - 9;
                                    expected = AudioDeviceTestAccess::value(*sourceCurves[owner][p], original);
                                }
                                savedPasteValues.push_back(expected);
                                compare(owner + 2, p, at, expected);
                            }
                    check(true, "actual copied and suffix volume pan EQ curves preserve gaps and full object envelope");
                    sourceCurves.clear();
                    destinationCurves.clear();
                    const auto after = persistentTracks(c);
                    const auto saved = dir.getChildFile("Paste-" + juce::String(pasteCases++) + ".tracktionedit");
                    c.save(saved);
                    c.undo(receipt["plan_id"]);
                    pump();
                    check(persistentTracks(c) == before, "object Shuffle Paste is one native Undo");
                    c.redo();
                    pump();
                    check(persistentTracks(c) == after, "object Shuffle Paste Redo exact");
                    open(saved);
                    check(sameSavedState(persistentTracks(c), after),
                          "object Shuffle Paste Save Open restores raw native state");
                    size_t savedIndex = 0;
                    for (int owner = 0; owner < 2; ++owner)
                        for (size_t p = 0; p < 3; ++p)
                            for (int i = 0; i < 512; ++i)
                                compare(owner + 2, p, std::llround(30. * i / 511. * 48000) / 48000.,
                                        savedPasteValues.at(savedIndex++));
                    check(true, "actual pasted native curves retain their DSP values after Save Open");
                }
        // Non-ripple whole-object Delete removes point membership, without invented Cut anchors.
        open(step);
        setBasis("samples");
        auto slipBuffer = copyObjects();
        const auto slipBefore = persistentTracks(c);
        auto slipPlan = c.makePlan(
            "human",
            Json::array({operation("timeline.clips.erase",
                                   {{"clipboard", slipBuffer["id"]}, {"action", "delete"}, {"ripple", false}})}));
        const auto slipPreview = c.preview(slipPlan)["timeline_changes"][0];
        check(slipPreview["action"] == "delete" && slipPreview["object_intervals"].empty(),
              "Slip Delete has explicit point-removal policy without timeline collapse");
        for (const auto& owner : slipPreview["automation"])
            for (const auto& lane : owner["lanes"])
            {
                Json expected = Json::array();
                for (const auto& point : lane["before"])
                {
                    bool removed = false;
                    for (const auto& interval : owner["intervals"])
                        removed |=
                            point["time_seconds"].get<double>() >= interval["start_samples"].get<int64_t>() / 48000. &&
                            point["time_seconds"].get<double>() < interval["end_samples"].get<int64_t>() / 48000.;
                    if (!removed)
                        expected.push_back(point);
                }
                check(lane["after"] == expected,
                      "Slip object Delete preserves all surviving raw points and does not create Cut anchors");
            }
        auto slipReceipt = c.commit(slipPlan);
        pump();
        for (const auto& owner : slipBefore)
            for (const auto& item : owner["clips"])
                if (std::find(selected.begin(), selected.end(), item["id"]) == selected.end())
                    check(clip(c, item["id"]) == item,
                          "Slip whole-object Delete leaves unselected native media positions and events exact");
        c.undo(slipReceipt["plan_id"]);
        pump();
        check(persistentTracks(c) == slipBefore, "Slip whole-object Delete is one native Undo");
        open(step);
        setBasis("samples");
        auto b = copyObjects();
        rejects(
            [&]
            {
                c.makePlan(
                    "human",
                    Json::array({operation("timeline.clips.erase",
                                           {{"clipboard", b["id"]}, {"action", "invented"}, {"ripple", true}})}));
            },
            "unknown erase action cannot be successful");
        rejects(
            [&]
            {
                c.makePlan(
                    "agent:test",
                    Json::array({operation("timeline.clips.erase",
                                           {{"clipboard", b["id"]}, {"ripple", true}, {"ripple_mapping", "native"}})}));
            },
            "object editing does not enlarge frozen Agent privileges");
        run(c, Json::array({operation("clip.import", {{"track", owners[0]},
                                                      {"path", media[2].getFullPathName().toStdString()},
                                                      {"media_hash", hashes[2]},
                                                      {"position_samples", 2 * 48000}})}));
        const auto overlapState = persistentTracks(c);
        rejects(
            [&]
            {
                c.makePlan(
                    "human",
                    Json::array({operation("timeline.clips.erase",
                                           {{"clipboard", b["id"]}, {"ripple", true}, {"ripple_mapping", "native"}})}));
            },
            "unselected overlapping media refuses whole object Shuffle");
        check(persistentTracks(c) == overlapState, "overlap refusal leaves the entire native project unchanged");
        open(step);
        setBasis("samples");
        b = copyObjects();
        rejects(
            [&]
            {
                c.makePlan("human", Json::array({operation(
                                        "timeline.clips.erase",
                                        {{"clipboard", b["id"]}, {"ripple", true}, {"ripple_mapping", "samples"}})}));
            },
            "sample ripple across changing Tempo Meter still refuses before any writes");
        auto stale = c.makePlan(
            "human",
            Json::array({operation("timeline.clips.erase",
                                   {{"clipboard", b["id"]}, {"ripple", true}, {"ripple_mapping", "native"}})}));
        run(c, Json::array({operation("clip.lock", {{"clip", suffixAudio}, {"locked", true}})}));
        const auto lockedState = persistentTracks(c);
        rejects([&] { c.commit(stale); }, "human suffix lock invalidates stale object plan");
        rejects(
            [&]
            {
                c.makePlan(
                    "human",
                    Json::array({operation("timeline.clips.erase",
                                           {{"clipboard", b["id"]}, {"ripple", true}, {"ripple_mapping", "native"}})}));
            },
            "locked remaining audio cannot silently move");
        check(persistentTracks(c) == lockedState, "lock failure preserves selected objects and every curve");
        auto selectObjects = [&]
        {
            AudioDeviceTestAccess::select(w, owners[0]);
            pump();
            Json objects = Json::array();
            const auto selectionFacts = c.query();
            for (const auto& track : selectionFacts["tracks"])
                for (const auto& f : track["clips"])
                    if (std::find(selected.begin(), selected.end(), f["id"]) != selected.end())
                        objects.push_back({{"id", f["id"]}, {"track", track["id"]}, {"kind", "clip"}});
            check(objects.size() == selected.size(), "GUI selection resolves every real owner from native query");
            c.updateUiState({{"object_selection", objects}, {"selection_tracks", Json::array({owners[0], owners[1]})}},
                            c.sessionToken());
            pump();
        };
        // GUI Cut/Delete, rejected preview retains original RAM clipboard, one native Undo.
        const juce::KeyPress custom(
            'j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        for (const bool deleting : {false, true})
        {
            open(step);
            setBasis("samples");
            selectObjects();
            key(w, 'c');
            b = c.clipboard();
            w.uiCommands().invokeDirectly(editCommand::shuffleNative, false);
            pump();
            w.uiCommands().invokeDirectly(editCommand::shuffle, false);
            pump();
            const auto before = persistentTracks(c);
            auto dispatch = [&]
            {
                if (deleting)
                {
                    check(w.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey)),
                          "default whole-object Delete shortcut dispatched");
                    pump();
                }
                else
                    key(w, 'x');
            };
            dispatch();
            check(!AudioDeviceTestAccess::pending(w).is_null() &&
                      AudioDeviceTestAccess::preview(w).contains(juce::String::fromUTF8("保留间隙")),
                  "GUI previews native per-track interval union before whole-object clear");
            AudioDeviceTestAccess::confirm(w, false);
            pump();
            check(persistentTracks(c) == before && c.clipboard() == b,
                  "whole-object clear cancellation preserves original clipboard and native Edit");
            dispatch();
            AudioDeviceTestAccess::confirm(w, true);
            pump();
            const auto after = persistentTracks(c);
            check((c.clipboard()["id"] == b["id"]) == deleting,
                  "Delete retains original accepted clipboard while Cut accepts actual frozen source");
            const auto saved =
                dir.getChildFile(deleting ? "GuiObjectDelete.tracktionedit" : "GuiObjectCut.tracktionedit");
            c.save(saved);
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(persistentTracks(c) == before, "GUI whole-object clear is one native Undo after Save");
            open(saved);
            check(sameSavedState(persistentTracks(c), after), "GUI whole-object clear saved native project restores");
        }
        open(step);
        setBasis("beats");
        selectObjects();
        key(w, 'c');
        b = c.clipboard();
        AudioDeviceTestAccess::select(w, owners[2]);
        pump();
        w.uiCommands().invokeDirectly(editCommand::shuffleNative, false);
        pump();
        w.uiCommands().invokeDirectly(editCommand::shuffle, false);
        pump();
        run(c, Json::array({operation("session.range.set",
                                      {{"start_samples", 9 * 48000}, {"end_samples", 9 * 48000 + 24000}})}));
        c.updateUiState(
            {{"object_selection", Json::array()}, {"selection_tracks", Json::array({owners[2], owners[3]})}},
            c.sessionToken());
        pump();
        auto* keys = w.uiCommands().getKeyMappings();
        keys->clearAllKeyPresses(editCommand::paste);
        keys->addKeyPress(editCommand::paste, custom);
        pump();
        const auto pasteBefore = persistentTracks(c);
        check(w.keyPressed(custom), "custom object Shuffle Paste key dispatched");
        pump();
        check(!AudioDeviceTestAccess::pending(w).is_null(), "object clipboard Paste generates a real native preview");
        AudioDeviceTestAccess::confirm(w, true);
        pump();
        const auto pasteAfter = persistentTracks(c);
        const auto guiSaved = dir.getChildFile("GuiObjectPaste.tracktionedit");
        c.save(guiSaved);
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(persistentTracks(c) == pasteBefore, "GUI object Paste one native Undo after Save");
        open(guiSaved);
        check(sameSavedState(persistentTracks(c), pasteAfter) && keys->containsMapping(editCommand::paste, custom),
              "object Paste native state and custom key survive Open");
        open(step);
        setBasis("samples");
        selectObjects();
        w.uiCommands().invokeDirectly(editCommand::shuffleNative, false);
        pump();
        w.uiCommands().invokeDirectly(editCommand::shuffle, false);
        pump();
        keys->clearAllKeyPresses(editCommand::paste);
        keys->addKeyPress(editCommand::paste, custom);
        pump();
        const auto demo = dir.getChildFile("ObjectShuffleReady.tracktionedit");
        c.save(demo);
        for (size_t i = 0; i < media.size(); ++i)
            check(Commands::mediaHash(media[i]) == hashes[i],
                  "original media hash unchanged by every object operation");
        check(juce::SHA256(step).toHexString() == stepHash, "original native fixture bytes unchanged");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"clear_cases", clearCases},
                    {"paste_cases", pasteCases},
                    {"native_iterator_probes", probes},
                    {"exact_recovery_native_doubles", recoveryDoubles},
                    {"maximum_normalized_curve_error", maximumError},
                    {"maximum_saved_derived_beat_error", maximumSavedBeatRounding},
                    {"demo", demo.getFullPathName().toStdString()},
                    {"limits", "whole native audio/MIDI object Shuffle; selected interval union per track and full "
                               "paste envelope; sample-sync MIDI crossing changing Tempo, Warp and partial loops "
                               "remain unqualified; Mac locked, physical GUI/listening unexecuted"}};
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
