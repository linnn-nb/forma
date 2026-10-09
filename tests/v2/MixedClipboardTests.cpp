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
    explicit Storage(juce::File f) : PropertyStorage("Forma mixed clipboard qualification"), folder(f) {}
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
    const auto dir = parent.getChildFile("mixed-" + juce::Uuid().toString());
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
        check(writer->writeFromAudioSampleBuffer(signal, 0, signal.getNumSamples()), "known stereo PCM written");
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
            if (i == 0 || (i >= 4 && audio))
                ops.push_back(operation("clip.import", {{"track", ref},
                                                        {"path", media.getFullPathName().toStdString()},
                                                        {"media_hash", mediaHash},
                                                        {"position_samples", i < 4 ? 48000 : 240000}}));
            else if (i == 1 || (i >= 4 && !audio))
            {
                const std::string clipRef = "$c" + std::to_string(i);
                ops.push_back(operation("midi.clip.create", {{"track", ref},
                                                             {"ref", clipRef},
                                                             {"name", "Native performance"},
                                                             {"position_samples", i < 4 ? 48000 : 240000},
                                                             {"length_samples", i < 4 ? 192000 : 288000}}));
                ops.push_back(operation("midi.note.add", {{"clip", clipRef},
                                                          {"pitch", 67},
                                                          {"velocity", 78},
                                                          {"position_samples", i < 4 ? 72000 : 240000},
                                                          {"length_samples", i < 4 ? 144000 : 240000}}));
            }
        }
        run(c, ops);
        auto q = c.query();
        Json sourceTracks = Json::array(), destTracks = Json::array();
        for (int i = 0; i < 4; ++i)
        {
            sourceTracks.push_back(q["tracks"][i]["id"]);
            destTracks.push_back(q["tracks"][i + 4]["id"]);
        }
        const std::string audio = q["tracks"][0]["clips"][0]["id"], midi = q["tracks"][1]["clips"][0]["id"];
        run(c, Json::array({operation(
                   "clip.fx.insert",
                   {{"clip", audio}, {"media_hash", mediaHash}, {"type", te::EqualiserPlugin::xmlTypeName}})}));
        run(c, Json::array({operation(
                   "plugin.parameter",
                   {{"plugin", clip(c, audio)["plugins"][0]["id"]}, {"parameter", "Mid gain 1"}, {"value", 3.}})}));
        run(c, Json::array({operation("clip.fade", {{"clip", audio},
                                                    {"media_hash", mediaHash},
                                                    {"in_samples", 2400},
                                                    {"out_samples", 4800},
                                                    {"in_curve", "linear"},
                                                    {"out_curve", "linear"}})}));
        for (const auto& id : sourceTracks)
            run(c, Json::array({curve(id, 0, -24.), curve(id, 96000, -18., .75), curve(id, 144000, -6.),
                                curve(id, 432000, -12.)}));
        for (const auto& id : destTracks)
            run(c, Json::array({curve(id, 0, -30.), curve(id, 480000, -12.)}));
        const auto base = dir.getChildFile("Base.tracktionedit");
        c.save(base);
        auto xml = juce::XmlDocument::parse(base);
        byID(*xml, audio)->setAttribute("qualification_extra", "retained-audio-state");
        auto* seq = byID(*xml, midi)->getChildByName("SEQUENCE");
        byID(*xml, midi)->setAttribute("qualification_extra", "retained-MIDI-state");
        check(seq != nullptr, "actual native MIDI sequence found in owned fixture");
        for (auto [beat, val] : {std::pair{.5, 32}, {2.5, 96}, {3.5, 64}})
            seq->addChildElement(
                te::MidiControllerEvent::createControllerEvent(tracktion::BeatPosition::fromBeats(beat), 1, val << 7, 0)
                    .createXml()
                    .release());
        const uint8_t payload[] = {0x7d, 0x12};
        seq->addChildElement(te::MidiSysexEvent::createSysexEvent(juce::MidiMessage::createSysExMessage(payload, 2),
                                                                  tracktion::BeatPosition::fromBeats(2.75))
                                 .createXml()
                                 .release());
        const auto fixture = dir.getChildFile("Fixture.tracktionedit");
        check(xml->writeTo(fixture), "owned mixed fixture saved");
        const auto fixtureHash = juce::SHA256(fixture).toHexString();
        w.openSession(fixture);
        pump();
        const auto before = persistentTracks(c);
        auto buffer = c.prepareTimelineRangeClipboard(sourceTracks, 96000, 144000, c.sessionToken(),
                                                      c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        check(buffer["kind"] == "timeline_clips" && buffer["tracks"].size() == 4 && buffer["entries"].size() == 2 &&
                  buffer["range_timebase"] == "mixed" && buffer["automation"].size() == 4 &&
                  persistentTracks(c) == before,
              "read-only mixed capture retains full selection empty tracks and four actual fader curves");
        check(c.timelineClipPasteRange(buffer["id"], 288000)["end_samples"] == 384000,
              "common range covers one second audio and two musical seconds without truncation");
        const auto placements = c.timelineClipPasteExtent(buffer["id"], 288000);
        check(placements.size() == 2 && placements[0]["length_samples"] == 48000 &&
                  placements[1]["length_samples"] == 96000,
              "audio samples and native MIDI beats retain their respective durations at destination Tempo");
        Json args{
            {"clipboard", buffer["id"]}, {"tracks", destTracks}, {"position_samples", 288000}, {"mode", "replace"}};
        auto plan = c.makePlan("human", Json::array({operation("timeline.clips.paste", args)}));
        auto preview = c.preview(plan);
        check(preview["timeline_changes"].size() == 1 && preview["midi_changes"].empty() &&
                  preview["timeline_changes"][0]["clips"].size() == 10 &&
                  preview["timeline_changes"][0]["automation"].size() == 4,
              "mixed preview reports all destination boundaries real inserts and empty-lane curves");
        Scope scope;
        scope.targets = destTracks.get<std::set<std::string>>();
        c.review(plan, scope);
        scope.targets.erase(destTracks.back().get<std::string>());
        rejects([&] { c.review(plan, scope); }, "permissions include silent destination track");
        rejects([&] { c.makePlan("ai:test", Json::array({operation("timeline.clips.paste", args)})); },
                "frozen Agent surface cannot invoke local-only timeline edits");
        double curveError = 0;
        bool curvesWithinBudget = true;
        std::vector<std::vector<float>> expected(4);
        for (int t = 0; t < 4; ++t)
            for (int i = 0; i < 2048; ++i)
            {
                const double at = 6. + 2. * i / 2048;
                const double src = t % 2 == 0 ? std::min(2. + at - 6., 3. - 1. / 48000) : 2. + (at - 6.) / 2.;
                expected[t].push_back(AudioDeviceTestAccess::value(c, sourceTracks[t], src));
            }
        const auto receipt = c.commit(plan);
        const auto copies = pasted(receipt);
        check(copies.size() == 2 && c.commit(plan)["replayed"] == true,
              "mixed paste inserts once and retry is idempotent");
        const auto pastedEq = clip(c, copies[0])["plugins"][0];
        bool eqPreserved = false;
        for (const auto& parameter : pastedEq["parameters"])
            if (parameter["id"] == "Mid gain 1")
                eqPreserved = std::abs(parameter["value"].get<double>() - 3.) < 1e-6;
        check(eqPreserved && pastedEq["id"] != clip(c, audio)["plugins"][0]["id"],
              "native audio clone retains real Clip EQ parameter with new plugin instance ID");
        check(clip(c, copies[0])["source_sample_rate"] == 44100 && clip(c, copies[0])["start_samples"] == 288000 &&
                  clip(c, copies[0])["length_samples"] == 48000 && clip(c, copies[0])["source_offset_seconds"] == 1. &&
                  clip(c, copies[0])["fade_in_samples"] == 0 && clip(c, copies[0])["fade_out_samples"] == 0,
              "real audio fragment keeps source frame rate offset and resets only new interior fade edges");
        check(clip(c, copies[1])["controller_events"].size() == 3 && clip(c, copies[1])["sysex_count"] == 1,
              "mixed MIDI fragment retains full real CC and SysEx source tree");
        for (int t = 0; t < 4; ++t)
            for (int i = 0; i < 2048; ++i)
            {
                const double error = std::abs(
                    double(AudioDeviceTestAccess::value(c, destTracks[t], 6. + 2. * i / 2048) - expected[t][i]));
                const double ulp =
                    std::max(std::nextafter(expected[t][i], std::numeric_limits<float>::infinity()) - expected[t][i],
                             expected[t][i] - std::nextafter(expected[t][i], -std::numeric_limits<float>::infinity()));
                curveError = std::max(curveError, error);
                curvesWithinBudget = curvesWithinBudget && error <= 1e-7 + 2 * ulp;
            }
        check(curvesWithinBudget,
              "actual native automation iterator matches per-track clocks and held silent tail within fixed budget");
        const auto saved = dir.getChildFile("MixedPaste.tracktionedit");
        c.save(saved);
        check(nativeData(saved, copies[0]) == nativeData(fixture, audio) &&
                  nativeData(saved, copies[1]) == nativeData(fixture, midi),
              "saved mixed native clones retain opaque audio fields and every MIDI source event");
        const auto pastedTracks = persistentTracks(c);
        c.undo(receipt["plan_id"]);
        if (persistentTracks(c) != before)
            std::cout << "UNDO_DIFF " << Json::diff(before, persistentTracks(c)).dump() << std::endl;
        check(persistentTracks(c) == before, "one Undo after save restores four destination tracks and curves");
        c.redo();
        check(persistentTracks(c) == pastedTracks, "one Redo restores stable native pasted IDs and curves");
        w.openSession(saved);
        pump();
        check(persistentTracks(c) == pastedTracks, "mixed paste state survives save Open");
        run(c, Json::array({operation("track.solo", {{"track", destTracks[0]}, {"enabled", true}})}));
        const auto actual = render(c, dir.getChildFile("PastedAudio.wav"), 288000, 384000);
        w.openSession(fixture);
        pump();
        run(c, Json::array({operation("track.solo", {{"track", sourceTracks[0]}, {"enabled", true}})}));
        const auto reference = render(c, dir.getChildFile("ReferenceAudio.wav"), 96000, 144000);
        double pcmError = 0, silentPeak = 0;
        for (int ch = 0; ch < 2; ++ch)
        {
            // Existing audio-node edge smoothing affects only the cut seam; compare the same signal interior.
            for (int i = 2048; i < 48000 - 2048; ++i)
                pcmError = std::max(pcmError, std::abs(double(actual.getSample(ch, i) - reference.getSample(ch, i))));
            for (int i = 48000 + 2048; i < 96000; ++i)
                silentPeak = std::max(silentPeak, std::abs(double(actual.getSample(ch, i))));
        }
        check(pcmError <= 2e-5 && silentPeak <= 1e-7,
              "actual resampled stereo paste matches frozen range render and longer envelope tail is silent");
        w.openSession(saved);
        pump();
        // The right retained destination fragment legitimately resumes its old
        // performance after 8 s. Remove only the two original boundary fragments
        // through L1 to qualify the copied held note's actual release in isolation.
        Json boundaryIDs = Json::array();
        const auto instrumentState = c.query();
        for (const auto& part : instrumentState["tracks"][5]["clips"])
            if (part["id"] != copies[1])
                boundaryIDs.push_back(part["id"]);
        auto boundaries = c.prepareMidiClipClipboard(boundaryIDs, c.sessionToken(), c.querySummary()["revision"]);
        run(c, Json::array({operation("midi.clips.erase", {{"clipboard", boundaries["id"]}})}));
        run(c, Json::array({operation("track.solo", {{"track", destTracks[1]}, {"enabled", true}})}));
        const auto instrument = render(c, dir.getChildFile("PastedInstrument.wav"), 288000, 432000);
        check(instrument.getRMSLevel(0, 2048, 80000) > 1e-5 && instrument.getRMSLevel(0, 120000, 24000) < 1e-6,
              "real FourOsc MIDI fragment sounds and releases after musical range without stuck note");
        w.openSession(fixture);
        pump();
        buffer = c.prepareTimelineRangeClipboard(sourceTracks, 96000, 144000, c.sessionToken(),
                                                 c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        const auto cut = run(c, Json::array({operation("timeline.clips.erase", {{"clipboard", buffer["id"]}})}));
        q = c.query();
        check(q["tracks"][0]["clips"].size() == 2 && q["tracks"][1]["clips"].size() == 2 &&
                  q["tracks"][0]["clips"][0]["fade_in_samples"] == 2400 &&
                  q["tracks"][0]["clips"][1]["fade_out_samples"] == 4800,
              "mixed Cut retains both original edges fades and native audio MIDI boundary fragments");
        const auto cutSaved = dir.getChildFile("MixedCut.tracktionedit");
        c.save(cutSaved);
        c.undo(cut["plan_id"]);
        check(persistentTracks(c) == before, "mixed Cut and all four source curves share one native Undo");
        c.redo();
        const auto cutTracks = persistentTracks(c);
        w.openSession(cutSaved);
        pump();
        check(persistentTracks(c) == cutTracks, "mixed Cut save Open preserves both media fragment types");
        w.openSession(fixture);
        pump();
        // Object selection keeps each native clock, even when no range is selected.
        buffer =
            c.prepareTimelineClipClipboard(Json::array({audio, midi}), c.sessionToken(), c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        const auto whole = c.timelineClipPasteExtent(buffer["id"], 288000);
        check(whole[0]["length_samples"] == 192000 && whole[1]["length_samples"] == 336000 &&
                  c.timelineClipPasteRange(buffer["id"], 288000)["end_samples"] == 624000,
              "mixed whole objects preserve four audio seconds and seven MIDI beats across Tempo");
        const auto wholeCut = run(c, Json::array({operation("timeline.clips.erase", {{"clipboard", buffer["id"]}})}));
        check(persistentTracks(c)[0]["clips"].empty() && persistentTracks(c)[1]["clips"].empty(),
              "mixed object Cut removes only selected native objects");
        c.undo(wholeCut["plan_id"]);
        check(persistentTracks(c) == before, "one mixed object Undo restores source objects and curves");
        buffer = c.prepareTimelineRangeClipboard(Json::array({sourceTracks[2], sourceTracks[3]}), 96000, 144000,
                                                 c.sessionToken(), c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        args["clipboard"] = buffer["id"];
        args["tracks"] = Json::array({destTracks[2], destTracks[3]});
        const auto blank = run(c, Json::array({operation("timeline.clips.paste", args)}));
        check(buffer["entries"].empty() && persistentTracks(c)[6]["clips"].size() == 2 &&
                  persistentTracks(c)[7]["clips"].size() == 2,
              "entirely silent mixed range clears common destination envelope without fabricating clips");
        c.undo(blank["plan_id"]);
        check(persistentTracks(c) == before, "silent mixed range Undo restores existing audio and MIDI");
        buffer = c.prepareTimelineRangeClipboard(sourceTracks, 96000, 144000, c.sessionToken(),
                                                 c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        args["clipboard"] = buffer["id"];
        args["tracks"] = destTracks;
        auto wrong = args;
        wrong["tracks"] = Json::array({destTracks[1], destTracks[0], destTracks[2], destTracks[3]});
        rejects([&] { c.makePlan("human", Json::array({operation("timeline.clips.paste", wrong)})); },
                "audio to MIDI-only incompatible destination rejects whole edit");
        plan = c.makePlan("human", Json::array({operation("timeline.clips.paste", args)}));
        run(c, Json::array({operation("track.gain", {{"track", destTracks[0]}, {"db", -20.}})}));
        const auto newer = persistentTracks(c);
        rejects([&] { c.commit(plan); }, "mixed paste refuses stale version after newer human edit");
        check(persistentTracks(c) == newer, "stale mixed paste cannot overwrite newer human changes");
        c.undo();
        pump();
        run(c, Json::array({operation("clip.lock", {{"clip", audio}, {"media_hash", mediaHash}, {"locked", true}})}));
        auto locked = c.prepareTimelineRangeClipboard(sourceTracks, 96000, 144000, c.sessionToken(),
                                                      c.querySummary()["revision"]);
        rejects(
            [&]
            { c.makePlan("human", Json::array({operation("timeline.clips.erase", {{"clipboard", locked["id"]}})})); },
            "mixed Cut respects original locked audio clip");
        c.undo();
        pump();
        const auto accepted = c.clipboard();
        rejects(
            [&]
            {
                c.prepareTimelineRangeClipboard(sourceTracks, 144000, 96000, c.sessionToken(),
                                                c.querySummary()["revision"]);
            },
            "invalid mixed capture range rejected");
        check(c.clipboard() == accepted, "failed mixed capture retains last accepted clipboard");
        buffer = c.prepareTimelineRangeClipboard(sourceTracks, 96000, 144000, c.sessionToken(),
                                                 c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        args["clipboard"] = buffer["id"];
        juce::MemoryBlock original;
        check(media.loadFileAsData(original), "owned source bytes saved before fault injection");
        auto altered = original;
        static_cast<uint8_t*>(altered.getData())[altered.getSize() - 1] ^= 1;
        check(media.replaceWithData(altered.getData(), altered.getSize()), "owned media hash fault injected");
        rejects([&] { c.makePlan("human", Json::array({operation("timeline.clips.paste", args)})); },
                "changed real media refuses mixed Paste");
        rejects(
            [&]
            { c.makePlan("human", Json::array({operation("timeline.clips.erase", {{"clipboard", buffer["id"]}})})); },
            "changed real media refuses mixed Cut");
        check(media.replaceWithData(original.getData(), original.getSize()),
              "owned source bytes restored after injected fault");
        run(c, Json::array({operation("session.range.set", {{"start_samples", 96000}, {"end_samples", 144000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", sourceTracks}}, c.sessionToken());
        pump();
        key(w, 'c');
        check(c.clipboard()["kind"] == "timeline_clips" && c.clipboard()["tracks"].size() == 4,
              "shared CmdC captures mixed range and both empty tracks");
        key(w, 'x');
        check(persistentTracks(c)[0]["clips"].size() == 2 && persistentTracks(c)[1]["clips"].size() == 2,
              "shared CmdX cuts both actual media types through L1");
        c.undo();
        pump();
        const auto retainedClipboard = c.clipboard();
        const auto beforeDuplicate = persistentTracks(c);
        key(w, 'd');
        if (!AudioDeviceTestAccess::pending(w).is_null())
        {
            check(persistentTracks(c) == beforeDuplicate,
                  "large mixed Duplicate preview is read-only before acceptance");
            AudioDeviceTestAccess::confirm(w, true);
            pump();
        }
        check(persistentTracks(c)[0]["clips"].size() == 2 && persistentTracks(c)[1]["clips"].size() == 2,
              "shared CmdD duplicates both types without replacing source");
        check(c.clipboard() == retainedClipboard, "Duplicate preserves the prior accepted clipboard");
        c.undo();
        pump();
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", sourceTracks}}, c.sessionToken());
        pump();
        key(w, 'v', true);
        if (!AudioDeviceTestAccess::pending(w).is_null())
        {
            AudioDeviceTestAccess::confirm(w, true);
            pump();
        }
        check(persistentTracks(c)[0]["clips"].size() == 3 && persistentTracks(c)[1]["clips"].size() == 3,
              "shared Paste Original replaces mixed interval and retains both original sides");
        c.undo();
        pump();
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom(
            'j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        keys->clearAllKeyPresses(editCommand::paste);
        keys->addKeyPress(editCommand::paste, custom);
        pump();
        run(c, Json::array({operation("session.range.set", {{"start_samples", 288000}, {"end_samples", 384000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", destTracks}}, c.sessionToken());
        AudioDeviceTestAccess::select(w, destTracks[0]);
        pump();
        check(w.keyPressed(custom), "custom mixed Paste key dispatches production command");
        pump();
        check(!AudioDeviceTestAccess::pending(w).is_null() &&
                  AudioDeviceTestAccess::preview(w).contains(juce::String::fromUTF8("共同范围")),
              "high-impact mixed GUI paste shows real preview and explains common time envelope");
        const auto pendingState = persistentTracks(c);
        AudioDeviceTestAccess::confirm(w, false);
        pump();
        check(persistentTracks(c) == pendingState && AudioDeviceTestAccess::pending(w).is_null(),
              "rejecting mixed preview writes no project changes");
        check(w.keyPressed(custom), "custom key recreates mixed preview");
        pump();
        AudioDeviceTestAccess::confirm(w, true);
        pump();
        const auto acceptedDemo = dir.getChildFile("MixedClipboardAccepted.tracktionedit");
        c.save(acceptedDemo);
        const auto guiTracks = persistentTracks(c);
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(persistentTracks(c) == pendingState, "GUI accepted mixed preview is one actual Undo");
        w.openSession(acceptedDemo);
        pump();
        if (persistentTracks(c) != guiTracks)
            std::cout << "OPEN_DIFF " << Json::diff(guiTracks, persistentTracks(c)).dump() << std::endl;
        std::cout << "CUSTOM_KEY_RESTORED " << keys->containsMapping(editCommand::paste, custom) << std::endl;
        check(persistentTracks(c) == guiTracks && keys->containsMapping(editCommand::paste, custom),
              "accepted mixed GUI state and custom key survive Open");
        // A file does not pretend to persist the private RAM clipboard. Start
        // the user demonstration with the real source range ready for CmdC.
        w.openSession(fixture);
        pump();
        keys->clearAllKeyPresses(editCommand::paste);
        keys->addKeyPress(editCommand::paste, custom);
        pump();
        run(c, Json::array({operation("session.range.set", {{"start_samples", 96000}, {"end_samples", 144000}})}));
        c.updateUiState({{"object_selection", Json::array()},
                         {"selection_tracks", sourceTracks},
                         {"start_samples", 0},
                         {"span_samples", 576000},
                         {"first_row", 0}},
                        c.sessionToken());
        pump();
        const auto demo = dir.getChildFile("MixedClipboardReady.tracktionedit");
        c.save(demo);
        w.openSession(demo);
        pump();
        check(persistentTracks(c) == before && c.clipboard().is_null() &&
                  keys->containsMapping(editCommand::paste, custom),
              "user demo opens original real sources custom key and no fabricated persistent clipboard");
        key(w, 'c');
        check(c.clipboard()["kind"] == "timeline_clips" && c.clipboard()["tracks"].size() == 4,
              "first CmdC after reopening ready demo captures real mixed selection with empty tracks");
        check(Commands::mediaHash(media) == mediaHash && juce::SHA256(fixture).toHexString() == fixtureHash,
              "all non-destructive edits preserve original source media and owned fixture bytes");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"native_iterator_probes", 8192},
                    {"maximum_curve_error", curveError},
                    {"curve_error_budget", "1e-7 native fader span (1) plus two float ULP at each reference value"},
                    {"maximum_pcm_error", pcmError},
                    {"silent_peak", silentPeak},
                    {"demo", demo.getFullPathName().toStdString()},
                    {"limits",
                     "Object Shuffle, partial MIDI loops, warped audio and same-track mixed timebase automation "
                     "remain refused; physical GUI/listening unexecuted"}};
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
