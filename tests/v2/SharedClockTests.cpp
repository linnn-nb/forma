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
    explicit Storage(juce::File f) : PropertyStorage("Forma shared clock qualification"), folder(f) {}
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
    const auto dir = parent.getChildFile("shared-clock-" + juce::Uuid().toString());
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
        check(bool(writer), "real known stereo PCM writer created");
        juce::AudioBuffer<float> signal(2, 4 * 44100);
        for (int i = 0; i < signal.getNumSamples(); ++i)
        {
            signal.setSample(0, i, .1f * std::sin(float(i) * .047f));
            signal.setSample(1, i, .065f * std::cos(float(i) * .073f));
        }
        check(writer->writeFromAudioSampleBuffer(signal, 0, signal.getNumSamples()), "actual known PCM written");
        writer.reset();
        const auto mediaHash = Commands::mediaHash(media);
        run(c, Json::array(
                   {operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}}),
                    operation("tempo.event.create", {{"beat_position", 16.}, {"bpm", 180.}}),
                    operation("tempo.event.create", {{"beat_position", 32.}, {"bpm", 80.}}),
                    operation("meter.event.create", {{"beat_position", 20.}, {"numerator", 3}, {"denominator", 8}}),
                    operation("meter.event.create", {{"beat_position", 32.}, {"numerator", 4}, {"denominator", 4}}),
                    operation("track.create", {{"ref", "$s"}, {"name", "Mixed source"}, {"type", "instrument"}}),
                    operation("track.create", {{"ref", "$d"}, {"name", "Mixed destination"}, {"type", "instrument"}}),
                    operation("track.create", {{"ref", "$r"}, {"name", "Unselected reference"}, {"type", "audio"}}),
                    operation("track.create", {{"ref", "$f"}, {"name", "Folder"}, {"type", "folder"}}),
                    operation("plugin.insert", {{"track", "$s"}, {"type", "4bandEq"}}),
                    operation("plugin.insert", {{"track", "$d"}, {"type", "4bandEq"}}),
                    operation("clip.import", {{"track", "$s"},
                                              {"path", media.getFullPathName().toStdString()},
                                              {"media_hash", mediaHash},
                                              {"position_samples", 48000}}),
                    operation("clip.import", {{"track", "$d"},
                                              {"path", media.getFullPathName().toStdString()},
                                              {"media_hash", mediaHash},
                                              {"position_samples", 8 * 48000}}),
                    operation("clip.import", {{"track", "$d"},
                                              {"path", media.getFullPathName().toStdString()},
                                              {"media_hash", mediaHash},
                                              {"position_samples", 16 * 48000}}),
                    operation("clip.import", {{"track", "$r"},
                                              {"path", media.getFullPathName().toStdString()},
                                              {"media_hash", mediaHash},
                                              {"position_samples", 0}}),
                    operation("midi.clip.create", {{"track", "$s"},
                                                   {"ref", "$sm"},
                                                   {"name", "Shared source MIDI"},
                                                   {"position_samples", 48000},
                                                   {"length_samples", 3 * 48000}}),
                    operation("midi.note.add", {{"clip", "$sm"},

                                                {"position_samples", 48000},
                                                {"length_samples", 2 * 48000},
                                                {"pitch", 60},
                                                {"velocity", 90}}),
                    operation("midi.clip.create", {{"track", "$d"},
                                                   {"ref", "$dm"},
                                                   {"name", "Shared destination MIDI"},
                                                   {"position_samples", 8 * 48000},
                                                   {"length_samples", 6 * 48000}}),
                    operation("midi.note.add", {{"clip", "$dm"},

                                                {"position_samples", 8 * 48000},
                                                {"length_samples", 5 * 48000},
                                                {"pitch", 67},
                                                {"velocity", 95}}),
                    operation("midi.clip.create", {{"track", "$d"},
                                                   {"ref", "$lm"},
                                                   {"name", "Shared suffix MIDI"},
                                                   {"position_samples", 16 * 48000},
                                                   {"length_samples", 2 * 48000}}),
                    operation("midi.note.add", {{"clip", "$lm"},

                                                {"position_samples", 16 * 48000},
                                                {"length_samples", 48000},
                                                {"pitch", 64},
                                                {"velocity", 96}})}));
        auto q = c.query();
        const std::string source = q["tracks"][0]["id"], target = q["tracks"][1]["id"],
                          untouched = q["tracks"][2]["id"], folder = q["tracks"][3]["id"];
        auto findClip = [&](const std::string& owner, const char* kind, int64_t start)
        {
            const auto nativeQuery = c.query();
            for (const auto& t : nativeQuery["tracks"])
                if (t["id"] == owner)
                    for (const auto& v : t["clips"])
                        if (v["kind"] == kind && v["start_samples"] == start)
                            return v["id"].get<std::string>();
            throw std::runtime_error("native fixture clip missing");
        };
        const auto sm = findClip(source, "midi", 48000), lm = findClip(target, "midi", 16 * 48000),
                   la = findClip(target, "audio", 16 * 48000);
        auto eq = [&](const std::string& owner)
        {
            const auto nativeQuery = c.query();
            for (const auto& t : nativeQuery["tracks"])
                if (t["id"] == owner)
                    for (const auto& p : t["plugins"])
                        for (const auto& a : p["parameters"])
                            if (a["id"] == "Mid gain 1")
                                return p["id"].get<std::string>() + "::Mid gain 1";
            throw std::runtime_error("actual EQ parameter missing");
        };
        auto synth = [&](const std::string& owner)
        {
            const auto nativeQuery = c.query();
            for (const auto& t : nativeQuery["tracks"])
                if (t["id"] == owner)
                    for (const auto& p : t["plugins"])
                        if (p["type"] == "4osc")
                            return p["id"].get<std::string>();
            throw std::runtime_error("actual native synth missing");
        };
        const auto sourceSynth = synth(source), destinationSynth = synth(target);
        run(c, Json::array({operation("plugin.bypass", {{"plugin", sourceSynth}, {"bypassed", true}}),
                            operation("plugin.bypass", {{"plugin", destinationSynth}, {"bypassed", true}})}));
        const std::vector<std::string> sourceParams{"volume", "pan", eq(source)},
            targetParams{"volume", "pan", eq(target)};
        for (const auto& owner : {source, target})
        {
            const auto& parameters = owner == source ? sourceParams : targetParams;
            Json ops = Json::array();
            for (size_t p = 0; p < parameters.size(); ++p)
            {
                const std::vector<double> values = p == 0   ? std::vector<double>{-24, -18, -6, -12, -20}
                                                   : p == 1 ? std::vector<double>{-.5, .2, .6, -.3, -.4}
                                                            : std::vector<double>{-3, 3, -2, 2, -1};
                size_t n = 0;
                for (const int64_t at :
                     {int64_t{0}, int64_t{2 * 48000}, int64_t{3 * 48000}, int64_t{15 * 48000}, int64_t{24 * 48000}})
                    ops.push_back(operation("automation.point.add",
                                            {{"track", owner},
                                             {"parameter", parameters[p]},
                                             {"position_samples", at},
                                             {"value", values[n]},
                                             {"curve", (n++ % 2 ? 1. : -1.) * (owner == source ? .25 : .75)},
                                             {"ref", "$point" + std::to_string(p) + "-" + std::to_string(at)}}));
            }
            run(c, ops);
        }
        const auto base = dir.getChildFile("Base.tracktionedit");
        c.save(base);
        auto xml = juce::XmlDocument::parse(base);
        for (const auto& id : {sm, lm})
        {
            auto* native = byID(*xml, id);
            native->setAttribute("qualification_extra", "original-shared-clock-events");
            auto* seq = native->getChildByName("SEQUENCE");
            seq->addChildElement(
                te::MidiControllerEvent::createControllerEvent(tracktion::BeatPosition::fromBeats(.5), 1, 96 << 7, 0)
                    .createXml()
                    .release());
            const uint8_t bytes[]{0x7d, 0x21};
            seq->addChildElement(te::MidiSysexEvent::createSysexEvent(juce::MidiMessage::createSysExMessage(bytes, 2),
                                                                      tracktion::BeatPosition::fromBeats(.75))
                                     .createXml()
                                     .release());
        }
        const auto fixture = dir.getChildFile("Fixture.tracktionedit");
        check(xml->writeTo(fixture), "owned actual native MIDI event fixture written");
        const auto fixtureHash = juce::SHA256(fixture).toHexString();
        auto rampXml = juce::XmlDocument::parse(fixture);
        int ramps = 0;
        for (auto* event : rampXml->getChildByName("TEMPOSEQUENCE")->getChildIterator())
            if (event->hasTagName("TEMPO") &&
                (event->getDoubleAttribute("startBeat") == 8. || event->getDoubleAttribute("startBeat") == 16.))
            {
                event->setAttribute("curve", event->getDoubleAttribute("startBeat") == 8. ? .35 : -.25);
                ++ramps;
            }
        check(ramps == 2, "two real native ramp coefficients installed");
        const auto ramp = dir.getChildFile("RampFixture.tracktionedit");
        check(rampXml->writeTo(ramp), "owned ramp fixture written");
        juce::File active = fixture;
        auto open = [&]
        {
            w.openSession(active);
            pump();
        };
        auto setting = [&](const std::string& id, const std::string& basis)
        { return operation("track.automation_edit_basis.set", {{"track", id}, {"basis", basis}}); };
        auto selectBasis = [&](const std::string& id, const std::string& basis)
        { run(c, Json::array({setting(id, basis)})); };
        auto copy = [&]
        {
            auto b = c.prepareTimelineRangeClipboard(Json::array({source}), 2 * 48000, 3 * 48000, c.sessionToken(),
                                                     c.querySummary()["revision"]);
            c.acceptClipboard(b["id"]);
            return b;
        };
        auto beatAt = [&](double s)
        { return AudioDeviceTestAccess::sequence(c).toBeats(tracktion::TimePosition::fromSeconds(s)).inBeats(); };
        auto timeAt = [&](double b)
        { return AudioDeviceTestAccess::sequence(c).toTime(tracktion::BeatPosition::fromBeats(b)).inSeconds(); };
        auto paste = [&](const Json& b, int64_t removal)
        {
            return c.makePlan(
                "human", Json::array({operation("timeline.clips.paste", {{"clipboard", b["id"]},
                                                                         {"tracks", Json::array({target})},
                                                                         {"position_samples", 9 * 48000},
                                                                         {"removal_end_samples", 9 * 48000 + removal},
                                                                         {"mode", "shuffle"},
                                                                         {"ripple_mapping", "native"}})}));
        };
        open();
        check(c.automationEditBasis(source) == "auto", "old mixed projects preserve automatic inference");
        const auto beforeRefusal = persistentTracks(c);
        rejects(copy, "mixed shared source in automatic mode rejects ambiguous mapping");
        check(persistentTracks(c) == beforeRefusal, "ambiguous Copy preserves native project");
        rejects([&] { c.makePlan("human", Json::array({setting(folder, "beats")})); },
                "folder has no clip curve edit basis");
        rejects([&] { c.makePlan("agent:test", Json::array({setting(source, "beats")})); },
                "new local setting does not enlarge Agent permissions");
        rejects([&] { c.makePlan("human", Json::array({setting(source, "bogus")})); },
                "unknown basis cannot become successful setting");
        rejects([&]
                { c.makePlan("human", Json::array({setting(source, "beats"), operation("session.range.clear", {})})); },
                "basis setting is a standalone transaction");
        auto settingPlan = c.makePlan("human", Json::array({setting(source, "beats")}));
        Scope partial;
        partial.targets = {target};
        rejects([&] { c.review(settingPlan, partial); }, "setting Scope enforces actual owning track");
        partial.targets = {source};
        partial.end = 48000;
        rejects([&] { c.review(settingPlan, partial); }, "basis change requires full-track time Scope");
        auto receipt = c.commit(settingPlan);
        pump();
        auto stable = c.save(dir.getChildFile("BasisSaved.tracktionedit"));
        (void)stable;
        c.undo(receipt["plan_id"]);
        pump();
        check(c.automationEditBasis(source) == "auto", "one Undo restores absent legacy metadata");
        c.redo();
        pump();
        check(c.automationEditBasis(source) == "beats", "basis Redo restores explicit mapping");
        auto frozenCopy = copy();
        selectBasis(source, "samples");
        check(c.clipboard()["track_timebases"][source] == "beats",
              "accepted clipboard keeps its actual frozen source mapping after setting change");
        selectBasis(target, "samples");
        check(c.preview(paste(frozenCopy, 0))["timeline_changes"][0]["automation"][0]["lanes"][0]["time_mapping"] ==
                  "native_musical",
              "real plan consumes frozen source curve basis despite later human setting changes");
        selectBasis(source, "auto");
        const auto reset = dir.getChildFile("BasisReset.tracktionedit");
        c.save(reset);
        auto resetXml = juce::XmlDocument::parse(reset);
        check(byID(*resetXml, source)->getChildByName("NDAW_AUTOMATION_EDIT_BASIS") == nullptr,
              "automatic basis removes explicit native metadata without removing track content");
        w.openSession(reset);
        pump();
        check(c.automationEditBasis(source) == "auto", "reset to legacy inference survives Open");
        open();
        selectBasis(source, "samples");
        auto b = copy();
        rejects([&] { paste(b, 24000); }, "ambiguous destination suffix still requires explicit shared curve basis");
        size_t probes = 0;
        double maximumError = 0;
        Json cases = Json::array();
        for (const auto& profile : {fixture, ramp})
        {
            active = profile;
            for (const std::string sourceBasis : {"samples", "beats"})
                for (const std::string destinationBasis : {"samples", "beats"})
                    for (const int64_t removal : {int64_t{0}, int64_t{24000}, int64_t{96000}, int64_t{192000}})
                    {
                        open();
                        selectBasis(source, sourceBasis);
                        selectBasis(target, destinationBasis);
                        b = copy();
                        const auto before = persistentTracks(c), oldMidi = clip(c, lm);
                        auto plan = paste(b, removal);
                        const auto preview = c.preview(plan)["timeline_changes"][0];
                        const double delta = preview["displacement_samples"].get<double>() / 48000.,
                                     beatDelta = preview["displacement_beats"];
                        const double end = preview["range"]["end_samples"].get<double>() / 48000.;
                        const double contentEnd =
                            sourceBasis == "samples" ? 10. : timeAt(beatAt(9.) + beatAt(3.) - beatAt(2.));
                        check(preview["automation_edit_bases"][target] == destinationBasis &&
                                  preview["automation"][0]["lanes"][0]["suffix_timebase"] == destinationBasis,
                              "preview seals declared shared curve clock and actual compiled suffix mapping");
                        std::vector<std::unique_ptr<te::AutomationCurve>> src, dst;
                        std::vector<double> spans;
                        for (size_t p = 0; p < 3; ++p)
                        {
                            src.push_back(AudioDeviceTestAccess::freeze(c, source, sourceParams[p]));
                            dst.push_back(AudioDeviceTestAccess::freeze(c, target, targetParams[p]));
                            spans.push_back(AudioDeviceTestAccess::span(c, target, targetParams[p]));
                        }
                        Scope scope;
                        scope.targets = {target};
                        c.review(plan, scope);
                        const auto result = c.commit(plan);
                        pump();
                        const auto firstState = persistentTracks(c);
                        auto replay = c.commit(plan);
                        check(replay["replayed"] == true && replay["objects"] == result["objects"] &&
                                  persistentTracks(c) == firstState,
                              "shared-clock retry returns replay receipt without duplicate clips or curve changes");
                        check(clip(c, la)["start_samples"] == std::llround((16. + delta) * 48000.) &&
                                  clip(c, lm)["start_samples"] ==
                                      std::llround(timeAt(beatAt(16.) + beatDelta) * 48000.),
                              "audio and musical MIDI retain independent actual native clip bases");
                        for (size_t p = 0; p < 3; ++p)
                            for (int i = 0; i < 1024; ++i)
                            {
                                const double at = std::llround((24. * i / 1023.) * 48000.) / 48000.;
                                double original = at;
                                bool sourcePart = false;
                                if (at >= 9. && at < end)
                                {
                                    sourcePart = true;
                                    const double copiedAt = std::min(at, contentEnd - 1. / 48000.);
                                    original = sourceBasis == "samples"
                                                   ? 2. + copiedAt - 9.
                                                   : timeAt(beatAt(2.) + beatAt(copiedAt) - beatAt(9.));
                                }
                                else if (at >= end)
                                    original =
                                        destinationBasis == "samples" ? at - delta : timeAt(beatAt(at) - beatDelta);
                                const float expected =
                                    AudioDeviceTestAccess::value(sourcePart ? *src[p] : *dst[p], original);
                                const float actual = AudioDeviceTestAccess::value(c, target, targetParams[p], at);
                                const double error = std::abs(double(actual) - expected);
                                maximumError = std::max(maximumError, error / spans[p]);
                                ++probes;
                                const double ulp = std::max(
                                    std::abs(double(std::nextafter(expected, std::numeric_limits<float>::infinity())) -
                                             expected),
                                    std::abs(double(std::nextafter(expected, -std::numeric_limits<float>::infinity())) -
                                             expected));
                                if (error > 1e-7 * spans[p] + 2 * ulp)
                                {
                                    std::cout << "CURVE_DIFF " << profile.getFileName() << ' ' << sourceBasis << ' '
                                              << destinationBasis << ' ' << removal << ' ' << p << ' ' << at << ' '
                                              << original << ' ' << expected << ' ' << actual << std::endl;
                                    throw std::runtime_error("shared-clock actual DSP curve exceeds fixed span budget");
                                }
                            }
                        check(true,
                              "volume pan and actual EQ native playback curves satisfy predetermined mapping budget");
                        const auto after = persistentTracks(c);
                        const auto saved = dir.getChildFile(
                            profile.getFileNameWithoutExtension() + "-" + juce::String(sourceBasis) + "-" +
                            juce::String(destinationBasis) + "-" + juce::String(removal) + ".tracktionedit");
                        c.save(saved);
                        check(nativeData(saved, lm) == nativeData(profile, lm),
                              "suffix raw MIDI CC SysEx and opaque fields remain exact");
                        for (const auto& id : pasted(result))
                            if (clip(c, id)["kind"] == "midi")
                                check(nativeData(saved, id) == nativeData(profile, sm),
                                      "copied native musical content is retained on shared track");
                        src.clear();
                        dst.clear();
                        c.undo(result["plan_id"]);
                        pump();
                        if (persistentTracks(c) != before)
                            std::cout << "UNDO_DIFF " << Json::diff(before, persistentTracks(c)).dump() << std::endl;
                        check(persistentTracks(c) == before, "entire shared-clock edit Undo is exact after Save");
                        c.redo();
                        pump();
                        check(persistentTracks(c) == after,
                              "shared-clock Redo restores actual IDs bases clips and curves");
                        w.openSession(saved);
                        pump();
                        check(sameSavedState(persistentTracks(c), after),
                              "shared-clock project survives Open with only bounded derived beat rounding");
                        check(c.automationEditBasis(source) == sourceBasis &&
                                  c.automationEditBasis(target) == destinationBasis,
                              "declared curve bases survive native persistence");
                        check(persistentTracks(c)[2] == before[2], "unselected actual reference track is untouched");
                        cases.push_back({{"profile", profile.getFileName().toStdString()},
                                         {"source_basis", sourceBasis},
                                         {"suffix_basis", destinationBasis},
                                         {"removed_samples", removal}});
                        if (profile == fixture && removal == 0 && sourceBasis == "samples")
                        {
                            run(c, Json::array({operation("track.solo", {{"track", target}, {"enabled", true}})}));
                            auto pcm = render(
                                c, dir.getChildFile("RealSharedAudio-" + juce::String(destinationBasis) + ".wav"),
                                std::llround((16. + delta) * 48000.), std::llround((20. + delta) * 48000.));
                            check(pcm.getMagnitude(0, pcm.getNumSamples()) > .001,
                                  "actual shared-track audio drives native EQ volume and pan in decoded WAV");
                            run(c, Json::array({operation("plugin.bypass",
                                                          {{"plugin", destinationSynth}, {"bypassed", false}})}));
                            const auto moved = clip(c, lm);
                            auto midiPcm =
                                render(c, dir.getChildFile("RealSharedMidi-" + juce::String(destinationBasis) + ".wav"),
                                       moved["start_samples"],
                                       moved["start_samples"].get<int64_t>() + moved["length_samples"].get<int64_t>());
                            check(midiPcm.getMagnitude(0, midiPcm.getNumSamples()) > .001,
                                  "actual retained MIDI drives an enumerated native instrument in separately decoded "
                                  "WAV");
                        }
                    }
        }
        active = fixture;
        for (const std::string basis : {"samples", "beats"})
        {
            open();
            selectBasis(target, basis);
            b = c.prepareTimelineRangeClipboard(Json::array({target}), 9 * 48000, 10 * 48000, c.sessionToken(),
                                                c.querySummary()["revision"]);
            c.acceptClipboard(b["id"]);
            auto plan = c.makePlan(
                "human",
                Json::array({operation("timeline.clips.erase",
                                       {{"clipboard", b["id"]}, {"ripple", true}, {"ripple_mapping", "native"}})}));
            const auto before = persistentTracks(c);
            std::vector<std::unique_ptr<te::AutomationCurve>> curves;
            for (const auto& p : targetParams)
                curves.push_back(AudioDeviceTestAccess::freeze(c, target, p));
            const double deltaB = beatAt(9.) - beatAt(10.);
            auto r = c.commit(plan);
            pump();
            for (size_t p = 0; p < 3; ++p)
                for (int i = 0; i < 512; ++i)
                {
                    const double at = std::llround((24. * i / 511.) * 48000.) / 48000.;
                    const double original = at < 9. ? at : basis == "samples" ? at + 1. : timeAt(beatAt(at) - deltaB);
                    const float expected = AudioDeviceTestAccess::value(*curves[p], original),
                                actual = AudioDeviceTestAccess::value(c, target, targetParams[p], at);
                    const auto span = AudioDeviceTestAccess::span(c, target, targetParams[p]);
                    const double ulp =
                        std::abs(double(std::nextafter(expected, std::numeric_limits<float>::infinity())) - expected);
                    const double error = std::abs(double(actual) - expected);
                    ++probes;
                    maximumError = std::max(maximumError, error / span);
                    if (error > 1e-7 * span + 2 * ulp)
                        throw std::runtime_error("shared range collapse exceeds predetermined native DSP span budget");
                }
            check(true, "shared range collapse follows declared native DSP curve basis");
            curves.clear();
            const auto after = persistentTracks(c);
            const auto saved = dir.getChildFile("Cut-" + juce::String(basis) + ".tracktionedit");
            c.save(saved);
            c.undo(r["plan_id"]);
            pump();
            check(persistentTracks(c) == before, "shared range collapse is one native Undo");
            c.redo();
            pump();
            check(persistentTracks(c) == after, "shared range collapse Redo exact");
            w.openSession(saved);
            pump();
            check(sameSavedState(persistentTracks(c), after), "shared collapse saved native state restored");
        }
        open();
        selectBasis(source, "samples");
        selectBasis(target, "samples");
        b = copy();
        auto stale = paste(b, 24000);
        selectBasis(target, "beats");
        auto freshState = persistentTracks(c);
        rejects([&] { c.commit(stale); }, "human basis change invalidates pending planning revision");
        check(persistentTracks(c) == freshState, "stale plan cannot overwrite human basis or curves");
        // Native GUI commands, actual default keys, per-project custom key persistence.
        open();
        AudioDeviceTestAccess::select(w, source);
        pump();
        const juce::KeyPress sampleKey(juce::KeyPress::F6Key, juce::ModifierKeys::altModifier, 0),
            beatKey(juce::KeyPress::F7Key, juce::ModifierKeys::altModifier, 0);
        check(w.keyPressed(beatKey), "default selected-track curve basis key reaches local L1");
        pump();
        check(c.automationEditBasis(source) == "beats", "GUI selected source basis is actual native fact");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(c.automationEditBasis(source) == "auto", "GUI setting Undo restores legacy inference");
        check(w.keyPressed(beatKey), "GUI basis key reapplies after Undo");
        pump();
        run(c,
            Json::array({operation("session.range.set", {{"start_samples", 2 * 48000}, {"end_samples", 3 * 48000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({source})}},
                        c.sessionToken());
        pump();
        key(w, 'c');
        b = c.clipboard();
        check(b["kind"] == "timeline_clips" && b["track_timebases"][source] == "beats",
              "CmdC actually freezes the configured mixed source curve basis");
        AudioDeviceTestAccess::select(w, target);
        pump();
        check(w.keyPressed(sampleKey), "default key sets target shared curves to samples");
        pump();
        w.uiCommands().invokeDirectly(editCommand::shuffleNative, false);
        pump();
        w.uiCommands().invokeDirectly(editCommand::shuffle, false);
        pump();
        run(c, Json::array({operation("session.range.set",
                                      {{"start_samples", 9 * 48000}, {"end_samples", 9 * 48000 + 24000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({target})}},
                        c.sessionToken());
        pump();
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom(
            'j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        keys->clearAllKeyPresses(editCommand::paste);
        keys->addKeyPress(editCommand::paste, custom);
        pump();
        const auto guiBefore = persistentTracks(c);
        check(w.keyPressed(custom), "custom shared native Paste shortcut dispatched");
        pump();
        check(!AudioDeviceTestAccess::pending(w).is_null() &&
                  AudioDeviceTestAccess::preview(w).contains(juce::String::fromUTF8("共享曲线跟随")),
              "GUI preview visibly declares independent shared curve basis");
        w.uiCommands().invokeDirectly(editCommand::curveBasisBeats, false);
        pump();
        check(c.automationEditBasis(target) == "samples", "pending GUI preview prevents conflicting basis edits");
        AudioDeviceTestAccess::confirm(w, false);
        pump();
        check(persistentTracks(c) == guiBefore && c.clipboard() == b,
              "preview rejection retains actual project and frozen clipboard");
        check(w.keyPressed(custom), "custom key replans shared clock preview");
        pump();
        AudioDeviceTestAccess::confirm(w, true);
        pump();
        const auto guiAfter = persistentTracks(c);
        const auto guiSaved = dir.getChildFile("GuiSharedAccepted.tracktionedit");
        c.save(guiSaved);
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(persistentTracks(c) == guiBefore, "shared GUI acceptance is one native Undo after Save");
        w.openSession(guiSaved);
        pump();
        check(sameSavedState(persistentTracks(c), guiAfter) && keys->containsMapping(editCommand::paste, custom),
              "shared GUI state and custom key persist through Open");
        auto invalidXml = juce::XmlDocument::parse(guiSaved);
        auto* node = byID(*invalidXml, target)->getChildByName("NDAW_AUTOMATION_EDIT_BASIS");
        check(node != nullptr, "basis persisted in owning native track metadata");
        node->setAttribute("basis", "invented");
        const auto invalid = dir.getChildFile("InvalidBasis.tracktionedit");
        check(invalidXml->writeTo(invalid), "owned corrupt mapping fixture written");
        rejects([&] { c.open(invalid); }, "invalid saved basis rejects candidate Edit");
        check(sameSavedState(persistentTracks(c), guiAfter), "corrupt Open cannot replace valid project");
        // A pure audio clip may have a beat-mapped curve longer than its destination audio.
        open();
        run(c, Json::array({operation("track.create", {{"ref", "$audio"}, {"type", "audio"}, {"name", "Frozen audio"}}),
                            operation("clip.import", {{"track", "$audio"},
                                                      {"path", media.getFullPathName().toStdString()},
                                                      {"media_hash", mediaHash},
                                                      {"position_samples", 48000}})}));
        const std::string audioSource = c.query()["tracks"].back()["id"];
        run(c, Json::array({curve(audioSource, 0, -24., -.25), curve(audioSource, 2 * 48000, -18., .25),
                            curve(audioSource, 3 * 48000, -6., -.25), curve(audioSource, 15 * 48000, -12.)}));
        selectBasis(audioSource, "beats");
        auto audioCurve = AudioDeviceTestAccess::freeze(c, audioSource, "volume");
        auto audioBuffer = c.prepareTimelineRangeClipboard(Json::array({audioSource}), 2 * 48000, 3 * 48000,
                                                           c.sessionToken(), c.querySummary()["revision"]);
        c.acceptClipboard(audioBuffer["id"]);
        selectBasis(target, "samples");
        check(c.timelineClipPasteRange(audioBuffer["id"], 9 * 48000)["end_samples"] == 11 * 48000 &&
                  c.timelineClipPasteExtent(audioBuffer["id"], 9 * 48000)[0]["length_samples"] == 48000,
              "pure audio beat curve extends common envelope without stretching the audio");
        auto audioPlan =
            c.makePlan("human", Json::array({operation("timeline.clips.paste", {{"clipboard", audioBuffer["id"]},
                                                                                {"tracks", Json::array({target})},
                                                                                {"position_samples", 9 * 48000},
                                                                                {"removal_end_samples", 11 * 48000},
                                                                                {"mode", "replace"},
                                                                                {"ripple_mapping", "native"}})}));
        const auto audioBefore = persistentTracks(c);
        auto audioReceipt = c.commit(audioPlan);
        pump();
        for (int i = 0; i < 512; ++i)
        {
            const double at = 9. + i / 256.;
            const double original = timeAt(beatAt(2.) + beatAt(at) - beatAt(9.));
            const float expected = AudioDeviceTestAccess::value(*audioCurve, original);
            const double span = AudioDeviceTestAccess::span(c, target, "volume");
            const double error = std::abs(double(AudioDeviceTestAccess::value(c, target, at)) - expected);
            const double ulp =
                std::abs(double(std::nextafter(expected, std::numeric_limits<float>::infinity())) - expected);
            ++probes;
            maximumError = std::max(maximumError, error / span);
            if (error > 1e-7 * span + 2 * ulp)
                throw std::runtime_error("pure audio beat curve exceeds predetermined native DSP span budget");
        }
        check(true, "pure audio beat-following curve uses actual native musical mapping throughout its envelope");
        audioCurve.reset();
        c.undo(audioReceipt["plan_id"]);
        pump();
        check(persistentTracks(c) == audioBefore, "audio with musical curve is one native Undo");
        selectBasis(audioSource, "auto");
        const auto bridgeFixture = dir.getChildFile("AudioBridgeFixture.tracktionedit");
        c.save(bridgeFixture);
        for (const std::string basis : {"samples", "beats"})
        {
            w.openSession(bridgeFixture);
            pump();
            keys->clearAllKeyPresses(editCommand::paste);
            keys->addKeyPress(editCommand::paste, custom);
            pump();
            AudioDeviceTestAccess::select(w, audioSource);
            pump();
            w.uiCommands().invokeDirectly(editCommand::slip, false);
            pump();
            run(c, Json::array(
                       {operation("session.range.set", {{"start_samples", 2 * 48000}, {"end_samples", 3 * 48000}})}));
            c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({audioSource})}},
                            c.sessionToken());
            pump();
            key(w, 'c');
            const auto originalBuffer = c.clipboard();
            check(originalBuffer["kind"] == "audio", "production CmdC freezes the ordinary legacy audio clipboard");
            auto frozen = AudioDeviceTestAccess::freeze(c, audioSource, "volume");
            // Change the source after Copy; the bridge must consume the frozen original curve.
            run(c, Json::array({curve(audioSource, 2 * 48000 + 24000, -40.)}));
            const auto currentRevision = c.querySummary()["revision"].get<uint64_t>();
            rejects([&]
                    { c.prepareTimelineFromAudioClipboard(originalBuffer["id"], "wrong-session", currentRevision); },
                    "audio bridge rejects an unrelated session");
            rejects(
                [&]
                { c.prepareTimelineFromAudioClipboard(originalBuffer["id"], c.sessionToken(), currentRevision - 1); },
                "audio bridge rejects stale project revisions");
            rejects([&] { c.prepareTimelineFromAudioClipboard("missing", c.sessionToken(), currentRevision); },
                    "audio bridge requires an actual private clipboard token");
            auto staged = c.prepareTimelineFromAudioClipboard(originalBuffer["id"], c.sessionToken(), currentRevision);
            check(staged["adapted_from"] == originalBuffer["id"] &&
                      staged["track_timebases"][audioSource] == "samples" && c.clipboard() == originalBuffer,
                  "staging audio bridge declares its frozen sample basis and retains accepted clipboard");
            AudioDeviceTestAccess::select(w, target);
            pump();
            check(w.keyPressed(basis == "beats" ? beatKey : sampleKey),
                  "production key changes destination curve basis after Copy");
            pump();
            w.uiCommands().invokeDirectly(editCommand::shuffleNative, false);
            pump();
            w.uiCommands().invokeDirectly(editCommand::shuffle, false);
            pump();
            run(c, Json::array({operation("session.range.set",
                                          {{"start_samples", 9 * 48000}, {"end_samples", 9 * 48000 + 24000}})}));
            c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({target})}},
                            c.sessionToken());
            pump();
            const auto bridgeBefore = persistentTracks(c);
            check(w.keyPressed(custom),
                  "ordinary audio clipboard reaches native shared-track Paste through the custom key");
            pump();
            check(!AudioDeviceTestAccess::pending(w).is_null() &&
                      AudioDeviceTestAccess::preview(w).contains(juce::String::fromUTF8("共享曲线跟随")),
                  "bridge GUI displays a real native preview for a mixed destination");
            AudioDeviceTestAccess::confirm(w, false);
            pump();
            check(persistentTracks(c) == bridgeBefore && c.clipboard() == originalBuffer,
                  "bridge rejection preserves original accepted clipboard and native Edit");
            check(w.keyPressed(custom), "bridge Paste replans after cancellation");
            pump();
            AudioDeviceTestAccess::confirm(w, true);
            pump();
            check(c.clipboard()["kind"] == "timeline_clips" && c.clipboard()["adapted_from"] == originalBuffer["id"],
                  "successful native edit receipt promotes the staged bridge clipboard");
            for (int i = 0; i < 256; ++i)
            {
                const double at = 9. + i / 256.;
                const float expected = AudioDeviceTestAccess::value(*frozen, 2. + i / 256.);
                const double span = AudioDeviceTestAccess::span(c, target, "volume");
                const double error = std::abs(double(AudioDeviceTestAccess::value(c, target, at)) - expected);
                const double ulp =
                    std::abs(double(std::nextafter(expected, std::numeric_limits<float>::infinity())) - expected);
                ++probes;
                maximumError = std::max(maximumError, error / span);
                if (error > 1e-7 * span + 2 * ulp)
                    throw std::runtime_error("audio bridge recaptured source or exceeded fixed native DSP budget");
            }
            check(true, "actual bridge curve retains copied source values despite subsequent source edits");
            frozen.reset();
            const auto bridgeAfter = persistentTracks(c);
            const auto savedBridge = dir.getChildFile("Bridge-" + juce::String(basis) + ".tracktionedit");
            c.save(savedBridge);
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(persistentTracks(c) == bridgeBefore, "accepted audio bridge is one native Undo after Save");
            w.uiCommands().invokeDirectly(7, false);
            pump();
            check(persistentTracks(c) == bridgeAfter, "audio bridge Redo restores exact raw state and explicit bases");
            w.openSession(savedBridge);
            pump();
            check(sameSavedState(persistentTracks(c), bridgeAfter),
                  "audio bridge saved native Edit restores the exact qualified state");
        }
        // The pure MIDI API must honor an explicit sample curve independently of its beat clips.
        open();
        run(c, Json::array(
                   {operation("track.create", {{"ref", "$ms"}, {"type", "instrument"}, {"name", "MIDI curve source"}}),
                    operation("track.create",
                              {{"ref", "$md"}, {"type", "instrument"}, {"name", "MIDI curve destination"}}),
                    operation("midi.clip.create", {{"ref", "$mc"},
                                                   {"track", "$ms"},
                                                   {"name", "Native MIDI"},
                                                   {"position_samples", 48000},
                                                   {"length_samples", 3 * 48000}}),
                    operation("midi.note.add", {{"clip", "$mc"},
                                                {"position_samples", 48000},
                                                {"length_samples", 2 * 48000},
                                                {"pitch", 62},
                                                {"velocity", 80}})}));
        const auto midiQuery = c.query();
        const std::string midiSource = midiQuery["tracks"][midiQuery["tracks"].size() - 2]["id"],
                          midiTarget = midiQuery["tracks"].back()["id"];
        run(c, Json::array({curve(midiSource, 0, -24., -.25), curve(midiSource, 2 * 48000, -18., .25),
                            curve(midiSource, 3 * 48000, -6., -.25), curve(midiSource, 15 * 48000, -12.)}));
        selectBasis(midiSource, "samples");
        auto midiCurve = AudioDeviceTestAccess::freeze(c, midiSource, "volume");
        auto midiBuffer = c.prepareMidiRangeClipboard(Json::array({midiSource}), 2 * 48000, 3 * 48000, c.sessionToken(),
                                                      c.querySummary()["revision"]);
        c.acceptClipboard(midiBuffer["id"]);
        check(midiBuffer["kind"] == "midi_clips" && midiBuffer["track_timebases"][midiSource] == "samples",
              "pure MIDI clipboard freezes an explicit sample curve while retaining musical clips");
        auto midiPlan =
            c.makePlan("human", Json::array({operation("midi.clips.paste", {{"clipboard", midiBuffer["id"]},
                                                                            {"tracks", Json::array({midiTarget})},
                                                                            {"position_samples", 9 * 48000},
                                                                            {"removal_end_samples", 11 * 48000},
                                                                            {"mode", "replace"}})}));
        const auto midiBefore = persistentTracks(c);
        auto midiReceipt = c.commit(midiPlan);
        pump();
        for (int i = 0; i < 256; ++i)
        {
            const double at = 9. + i / 256.;
            const float expected = AudioDeviceTestAccess::value(*midiCurve, 2. + i / 256.);
            const double span = AudioDeviceTestAccess::span(c, midiTarget, "volume");
            const double error = std::abs(double(AudioDeviceTestAccess::value(c, midiTarget, at)) - expected);
            const double ulp =
                std::abs(double(std::nextafter(expected, std::numeric_limits<float>::infinity())) - expected);
            ++probes;
            maximumError = std::max(maximumError, error / span);
            if (error > 1e-7 * span + 2 * ulp)
                throw std::runtime_error("pure MIDI sample curve exceeds predetermined native DSP span budget");
        }
        check(true, "pure MIDI explicit sample curve uses actual sample mapping independently of musical clip length");
        midiCurve.reset();
        const auto midiAfter = persistentTracks(c);
        const auto midiSaved = dir.getChildFile("MidiExplicitSamples.tracktionedit");
        c.save(midiSaved);
        c.undo(midiReceipt["plan_id"]);
        pump();
        check(persistentTracks(c) == midiBefore, "pure MIDI explicit curve Paste is one native Undo");
        c.redo();
        pump();
        check(persistentTracks(c) == midiAfter, "pure MIDI explicit curve Redo exact");
        w.openSession(midiSaved);
        pump();
        check(sameSavedState(persistentTracks(c), midiAfter),
              "pure MIDI explicit curve mapping persists through native Save Open");
        open();
        selectBasis(source, "beats");
        selectBasis(target, "samples");
        w.uiCommands().invokeDirectly(editCommand::shuffleNative, false);
        pump();
        w.uiCommands().invokeDirectly(editCommand::shuffle, false);
        pump();
        keys->clearAllKeyPresses(editCommand::paste);
        keys->addKeyPress(editCommand::paste, custom);
        pump();
        AudioDeviceTestAccess::select(w, source);
        pump();
        run(c,
            Json::array({operation("session.range.set", {{"start_samples", 2 * 48000}, {"end_samples", 3 * 48000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({source})}},
                        c.sessionToken());
        pump();
        const auto demo = dir.getChildFile("SharedClockReady.tracktionedit");
        c.save(demo);
        check(Commands::mediaHash(media) == mediaHash && juce::SHA256(fixture).toHexString() == fixtureHash,
              "original media and native input fixture unchanged");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"cases", cases},
                    {"native_iterator_probes", probes},
                    {"maximum_normalized_curve_error", maximumError},
                    {"maximum_saved_derived_beat_error", maximumSavedBeatRounding},
                    {"demo", demo.getFullPathName().toStdString()},
                    {"limits", "explicit native clipboard and range Shuffle shared curves; auto ambiguous mapping "
                               "still refused; sample-sync MIDI across changing Tempo, object Shuffle, Warp remain "
                               "unqualified; physical GUI and listening unexecuted"}};
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
