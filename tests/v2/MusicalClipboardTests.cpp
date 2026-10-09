#include "ui/Workspace.h"
#include "AutomationCurveEdit.h"
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
    static tracktion::tempo::Sequence tempo(Commands& c)
    {
        c.edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(0));
        return c.edit->tempoSequence.getInternalSequence();
    }
    static float value(Commands& c, const std::string& id, double seconds)
    {
        auto* p = c.automationParameter(id, "volume");
        te::AutomationIterator iterator(*p);
        iterator.setPosition(tracktion::TimePosition::fromSeconds(seconds));
        return iterator.getCurrentValue();
    }
    static std::unique_ptr<te::AutomationIterator> playback(Commands& c, const std::string& id)
    {
        return std::make_unique<te::AutomationIterator>(*c.automationParameter(id, "volume"));
    }
    static float editorValue(Commands& c, const std::string& id, double seconds)
    {
        auto* p = c.automationParameter(id, "volume");
        return p->getCurve().getValueAt(tracktion::TimePosition::fromSeconds(seconds), p->getCurrentExplicitValue());
    }
    static double span(Commands& c, const std::string& id)
    {
        auto* p = c.automationParameter(id, "volume");
        return p->valueRange.end - p->valueRange.start;
    }
    static Json pending(Workspace& w)
    {
        return w.pending;
    }
    static void select(Workspace& w, const std::string& id)
    {
        w.select(id);
    }
    static juce::String previewText(Workspace& w)
    {
        return w.previewText.getText();
    }
    static void confirm(Workspace& w, bool accepted)
    {
        if (accepted)
            w.acceptButton.onClick();
        else
            w.rejectButton.onClick();
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
    juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
}
Json run(Commands& c, Json ops)
{
    auto result = c.commit(c.makePlan("human", std::move(ops)));
    check(result["state"] == "committed", "real native transaction committed");
    pump();
    return result;
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma musical curves"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
Json add(const std::string& track, int64_t samples, double value, double shape, const std::string& ref)
{
    return operation("automation.point.add", {{"track", track},
                                              {"parameter", "volume"},
                                              {"position_samples", samples},
                                              {"value", value},
                                              {"curve", shape},
                                              {"ref", ref}});
}
juce::XmlElement* byID(juce::XmlElement& node, const std::string& id)
{
    if (node.getStringAttribute("ndaw_id").toStdString() == id)
        return &node;
    for (auto* ch = node.getFirstChildElement(); ch; ch = ch->getNextElement())
        if (auto* result = byID(*ch, id))
            return result;
    return nullptr;
}
void ramps(juce::XmlElement& node)
{
    if (node.hasTagName("TEMPO"))
        node.setAttribute("curve", node.getDoubleAttribute("startBeat") == 0 ? .3 : -.35);
    for (auto* ch = node.getFirstChildElement(); ch; ch = ch->getNextElement())
        ramps(*ch);
}
Json points(Commands& c, const std::string& id)
{
    const auto query = c.automationQuery(id);
    for (const auto& lane : query["lanes"])
        if (lane["parameter"] == "volume")
            return lane["points"];
    throw std::runtime_error("fader points absent");
}
Json stable(Commands& c, const std::string& id)
{
    auto q = c.automationQuery(id)["lanes"];
    for (auto& lane : q)
        for (const auto* field : {"value", "explicit_value", "display", "recording"})
            lane.erase(field);
    return q;
}
void key(Workspace& w, char ch)
{
    check(w.keyPressed(juce::KeyPress(ch, juce::ModifierKeys::commandModifier, ch)),
          "production clipboard key handled");
    pump();
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto folder = parent.getChildFile("musical-curves-" + juce::Uuid().toString());
    folder.createDirectory();
    double maximumError = 0, editorGetterDiscrepancy = 0;
    Json cases = Json::array();
    try
    {
        Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1440, 900);
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c, Json::array(
                   {operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}}),
                    operation("tempo.event.create", {{"beat_position", 16.}, {"bpm", 180.}}),
                    operation("meter.event.create", {{"beat_position", 16.}, {"numerator", 7}, {"denominator", 8}}),
                    operation("track.create", {{"name", "Source performance"}, {"type", "midi"}, {"ref", "$s"}}),
                    operation("midi.clip.create", {{"track", "$s"},
                                                   {"name", "Musical source"},
                                                   {"ref", "$c"},
                                                   {"position_samples", 0},
                                                   {"length_samples", 192000}}),
                    operation("midi.note.add", {{"clip", "$c"},
                                                {"pitch", 60},
                                                {"velocity", 80},
                                                {"position_samples", 48000},
                                                {"length_samples", 96000}}),
                    operation("track.create", {{"name", "Mapped destination"}, {"type", "midi"}, {"ref", "$d"}})}));
        const auto q = c.query();
        const std::string source = q["tracks"][0]["id"], destination = q["tracks"][1]["id"];
        const auto base = folder.getChildFile("Base.tracktionedit");
        c.save(base);
        for (bool ramp : {false, true})
            for (double shape : {0., .25, -.25, .5, -.5, .75, -.75, 1., -1.})
            {
                const auto caseFolder = folder.getChildFile("case-" + juce::String(int(cases.size())));
                caseFolder.createDirectory();
                w.openSession(base);
                pump();
                run(c, Json::array({add(source, 0, -18., shape, "$a"), add(source, 96000, -6., -shape, "$b"),
                                    add(source, 192000, -12., 0., "$e"), add(destination, 0, -24., -.25, "$da"),
                                    add(destination, 960000, -9., 0., "$de")}));
                const auto raw = caseFolder.getChildFile("Raw.tracktionedit");
                c.save(raw);
                auto xml = juce::XmlDocument::parse(raw);
                if (ramp)
                    ramps(*xml);
                auto* marked = byID(*xml, points(c, source)[1]["id"]);
                check(marked != nullptr, "actual native POINT found in saved Edit");
                marked->setAttribute("qualification_origin", "frozen-real-point");
                const auto fixture = caseFolder.getChildFile("Case.tracktionedit");
                check(xml->writeTo(fixture), "owned native curve fixture saved");
                w.openSession(fixture);
                pump();
                if (shape == .25)
                {
                    editorGetterDiscrepancy = std::abs(double(AudioDeviceTestAccess::editorValue(c, source, .5)) -
                                                       AudioDeviceTestAccess::value(c, source, .5));
                    check(editorGetterDiscrepancy == 0,
                          "SDK editor getter exactly matches actual audio iterator after curved clipboard setup");
                }
                const auto frozenMap = AudioDeviceTestAccess::tempo(c);
                const int64_t first = 24000, last = 288000, target = 384000;
                const double origin = frozenMap.toBeats(tracktion::TimePosition::fromSeconds(first / 48000.)).inBeats();
                auto buffer = c.prepareMidiRangeClipboard(Json::array({source}), first, last, c.sessionToken(),
                                                          c.querySummary()["revision"]);
                c.acceptClipboard(buffer["id"]);
                check(buffer.contains("source_tempo_hash"), "clipboard records actual frozen Tempo map fingerprint");
                const auto prior = stable(c, destination), srcBefore = stable(c, source);
                auto args = Json{{"clipboard", buffer["id"]},
                                 {"tracks", Json::array({destination})},
                                 {"position_samples", target},
                                 {"mode", "replace"}};
                auto plan = c.makePlan("human", Json::array({operation("midi.clips.paste", args)}));
                const auto preview = c.preview(plan);
                const auto end = c.midiClipPasteRange(buffer["id"], target)["end_samples"].get<int64_t>();
                check(preview["midi_changes"][0]["automation"][0]["lanes"][0]["time_mapping"] == "native_musical",
                      "preview states actual musical curve mapping");
                Scope scope;
                scope.targets = {destination};
                scope.end = end;
                rejects([&] { c.review(plan, scope); }, "Scope protects destination curve outside selection");
                auto receipt = c.commit(plan);
                pump();
                check(c.commit(plan)["replayed"] == true, "musical paste retries cannot duplicate automation");
                const auto currentMap = AudioDeviceTestAccess::tempo(c);
                const double anchor =
                    currentMap.toBeats(tracktion::TimePosition::fromSeconds(target / 48000.)).inBeats();
                const double tolerance = AudioDeviceTestAccess::span(c, source) * 1e-7;
                double error = 0;
                auto sourcePlayback = AudioDeviceTestAccess::playback(c, source);
                auto destinationPlayback = AudioDeviceTestAccess::playback(c, destination);
                // Compare real native source/destination evaluators at every 13th
                // destination sample, plus both exact end guards. No production
                // curve projection helper is used for the expected values.
                std::set<int64_t> probes{target, end - 1};
                for (int64_t at = target; at < end; at += 13)
                    probes.insert(at);
                for (double sourceSeconds : {0., 2., 4.})
                {
                    const double mapped =
                        currentMap
                            .toTime(tracktion::BeatPosition::fromBeats(
                                anchor +
                                frozenMap.toBeats(tracktion::TimePosition::fromSeconds(sourceSeconds)).inBeats() -
                                origin))
                            .inSeconds() *
                        48000.;
                    for (int offset : {-1, 0, 1, 2})
                        if (const auto at = int64_t(std::floor(mapped)) + offset; at >= target && at < end)
                            probes.insert(at);
                }
                for (int64_t at : probes)
                {
                    const double music =
                        currentMap.toBeats(tracktion::TimePosition::fromSeconds(at / 48000.)).inBeats();
                    const double seconds =
                        frozenMap.toTime(tracktion::BeatPosition::fromBeats(origin + music - anchor)).inSeconds();
                    sourcePlayback->setPosition(tracktion::TimePosition::fromSeconds(seconds));
                    destinationPlayback->setPosition(tracktion::TimePosition::fromSeconds(at / 48000.));
                    const float expected = sourcePlayback->getCurrentValue(),
                                actual = destinationPlayback->getCurrentValue();
                    const double ulp =
                        std::abs(double(std::nextafter(expected, std::numeric_limits<float>::infinity())) - expected);
                    error = std::max(error, std::abs(double(actual) - expected));
                    if (std::abs(double(actual) - expected) > tolerance + 2 * ulp)
                        throw std::runtime_error(
                            "native musical curve error exceeded preset 1e-7 span plus two float ULP at " +
                            std::to_string(at) + " error " + std::to_string(std::abs(double(actual) - expected)));
                }
                sourcePlayback.reset();
                destinationPlayback.reset();
                maximumError = std::max(maximumError, error);
                check(true,
                      "real native playback iterators agree through Tempo/Meter changes within preset error budget");
                check(stable(c, source) == srcBefore, "source automation untouched by paste");
                const auto saved = caseFolder.getChildFile("Mapped.tracktionedit");
                c.save(saved);
                auto savedXml = juce::XmlDocument::parse(saved);
                bool attribute = false;
                for (const auto& p : points(c, destination))
                    if (auto* node = byID(*savedXml, p["id"]);
                        node && node->getStringAttribute("qualification_origin") == "frozen-real-point")
                        attribute = true;
                check(attribute, "actual copied POINT opaque attributes persist with new ID");
                const auto after = stable(c, destination);
                c.undo(receipt["plan_id"]);
                pump();
                check(stable(c, destination) == prior && c.query()["tracks"][1]["clips"].empty(),
                      "one Undo restores curve and MIDI after save");
                c.redo();
                pump();
                check(stable(c, destination) == after, "one Redo restores musical curve");
                w.openSession(saved);
                pump();
                check(stable(c, destination) == after, "Save/Open restores mapped native curves");
                cases.push_back(
                    {{"tempo_ramp", ramp}, {"shape", shape}, {"maximum_native_error", error}, {"end_samples", end}});
            }
        // Exact incoming step at destination zero: the next audible sample
        // must be the next DESTINATION sample after a Tempo change.
        w.openSession(base);
        pump();
        run(c, Json::array({add(source, 0, -18., 1., "$a"), add(source, 96000, -6., -.25, "$b"),
                            add(source, 192000, -12., 0., "$e")}));
        const auto zeroMap = AudioDeviceTestAccess::tempo(c);
        auto zeroPlayback = AudioDeviceTestAccess::playback(c, source);
        auto zeroBuffer = c.prepareMidiRangeClipboard(Json::array({source}), 96000, 192000, c.sessionToken(),
                                                      c.querySummary()["revision"]);
        c.acceptClipboard(zeroBuffer["id"]);
        run(c, Json::array({operation(
                   "tempo.event.set",
                   {{"event", c.query()["music"]["tempos"][0]["id"]}, {"beat_position", 0.}, {"bpm", 180.}})}));
        auto zeroReceipt = run(c, Json::array({operation("midi.clips.paste", {{"clipboard", zeroBuffer["id"]},
                                                                              {"tracks", Json::array({destination})},
                                                                              {"position_samples", 0},
                                                                              {"mode", "replace"}})}));
        const auto zeroDestinationMap = AudioDeviceTestAccess::tempo(c);
        auto zeroActual = AudioDeviceTestAccess::playback(c, destination);
        double zeroError = 0;
        const int64_t zeroEnd = c.midiClipPasteRange(zeroBuffer["id"], 0)["end_samples"];
        for (int64_t at = 0; at < zeroEnd; at += at < 3 ? 1 : 11)
        {
            const auto sourceTime = zeroMap.toTime(tracktion::BeatPosition::fromBeats(
                4. + zeroDestinationMap.toBeats(tracktion::TimePosition::fromSeconds(at / 48000.)).inBeats()));
            zeroPlayback->setPosition(sourceTime);
            zeroActual->setPosition(tracktion::TimePosition::fromSeconds(at / 48000.));
            const auto expected = zeroPlayback->getCurrentValue();
            const double error = std::abs(double(zeroActual->getCurrentValue()) - expected);
            const double ulp =
                std::abs(double(std::nextafter(expected, std::numeric_limits<float>::infinity())) - expected);
            if (error > AudioDeviceTestAccess::span(c, source) * 1e-7 + 2 * ulp)
                throw std::runtime_error("destination zero incoming step exceeded unchanged curve budget at " +
                                         std::to_string(at));
            zeroError = std::max(zeroError, error);
        }
        zeroPlayback.reset();
        zeroActual.reset();
        check(true, "destination zero incoming step and next destination sample preserve real playback");
        check(c.query()["tracks"][1]["clips"][0]["notes"][0]["position_samples"].get<int64_t>() < 0,
              "non-destructive source note before zero remains queryable as signed time");
        const auto zeroCurve = stable(c, destination);
        const auto zeroSaved = folder.getChildFile("ZeroBoundary.tracktionedit");
        c.save(zeroSaved);
        c.undo(zeroReceipt["plan_id"]);
        pump();
        check(c.query()["tracks"][1]["clips"].empty(), "zero-boundary mapped edit is one native Undo");
        c.redo();
        pump();
        check(stable(c, destination) == zeroCurve, "zero-boundary Redo restores guarded curve");
        w.openSession(zeroSaved);
        pump();
        check(stable(c, destination) == zeroCurve &&
                  c.query()["tracks"][1]["clips"][0]["notes"][0]["position_samples"].get<int64_t>() < 0,
              "zero-boundary curve and retained source event survive Save/Open");
        // Copy freezes the source map; later human Tempo edits affect only
        // destination planning and cannot reinterpret the copied curve.
        w.openSession(base);
        pump();
        run(c, Json::array({add(source, 0, -18., .25, "$a"), add(source, 96000, -6., -.25, "$b"),
                            add(source, 192000, -12., 0., "$e")}));
        const auto frozenMap = AudioDeviceTestAccess::tempo(c);
        auto sourcePlayback = AudioDeviceTestAccess::playback(c, source);
        auto buffer = c.prepareMidiRangeClipboard(Json::array({source}), 48000, 192000, c.sessionToken(),
                                                  c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        auto args = Json{{"clipboard", buffer["id"]},
                         {"tracks", Json::array({destination})},
                         {"position_samples", 288000},
                         {"mode", "replace"}};
        const auto stale = c.makePlan("human", Json::array({operation("midi.clips.paste", args)}));
        const std::string initialTempo = c.query()["music"]["tempos"][0]["id"];
        run(c, Json::array(
                   {operation("tempo.event.set", {{"event", initialTempo}, {"beat_position", 0.}, {"bpm", 180.}})}));
        rejects([&] { c.commit(stale); }, "human Tempo edit invalidates previously planned paste");
        const auto humanCurve = stable(c, source);
        check(points(c, source)[1]["position_samples"] == 64000 && points(c, source)[2]["position_samples"] == 128000,
              "native human Tempo edit remaps source automation by beats");
        auto live = AudioDeviceTestAccess::tempo(c);
        check(live.toBeats(tracktion::TimePosition::fromSeconds(2.)).inBeats() !=
                  frozenMap.toBeats(tracktion::TimePosition::fromSeconds(2.)).inBeats(),
              "actual source Tempo changed after Copy");
        const auto receipt = run(c, Json::array({operation("midi.clips.paste", args)}));
        const int64_t end = c.midiClipPasteRange(buffer["id"], 288000)["end_samples"];
        const double anchor = live.toBeats(tracktion::TimePosition::fromSeconds(6.)).inBeats();
        double frozenError = 0;
        auto destinationPlayback = AudioDeviceTestAccess::playback(c, destination);
        for (int64_t at = 288000; at < end; at += 17)
        {
            const auto music = live.toBeats(tracktion::TimePosition::fromSeconds(at / 48000.)).inBeats();
            const auto src = frozenMap.toTime(tracktion::BeatPosition::fromBeats(2. + music - anchor)).inSeconds();
            sourcePlayback->setPosition(tracktion::TimePosition::fromSeconds(src));
            destinationPlayback->setPosition(tracktion::TimePosition::fromSeconds(at / 48000.));
            const float expected = sourcePlayback->getCurrentValue(), actual = destinationPlayback->getCurrentValue();
            const double ulp =
                std::abs(double(std::nextafter(expected, std::numeric_limits<float>::infinity())) - expected);
            frozenError = std::max(frozenError, std::abs(double(actual) - expected));
            if (std::abs(double(actual) - expected) > AudioDeviceTestAccess::span(c, source) * 1e-7 + 2 * ulp)
                throw std::runtime_error("paste reinterpreted frozen source Tempo or exceeded curve budget");
        }
        sourcePlayback.reset();
        destinationPlayback.reset();
        check(true, "real destination uses frozen source map and current destination map");
        c.undo(receipt["plan_id"]);
        pump();
        check(stable(c, source) == humanCurve && c.query()["tracks"][1]["clips"].empty(),
              "Undo preserves newer human Tempo and source curve");
        check(c.query()["music"]["tempos"][0]["bpm"] == 180., "Undo does not erase intervening human Tempo edit");
        // Production main timeline Copy/Paste and Undo, not a separate command.
        AudioDeviceTestAccess::select(w, source);
        run(c, Json::array({operation("session.range.set", {{"start_samples", 48000}, {"end_samples", 144000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({source})}},
                        c.sessionToken());
        pump();
        key(w, 'c');
        check(c.clipboard().contains("source_tempo_hash"), "GUI Copy freezes actual native music map");
        const auto guiCurve = stable(c, source);
        run(c, Json::array({operation("session.range.set", {{"start_samples", 288000}, {"end_samples", 288001}})}));
        pump();
        key(w, 'v');
        check(!AudioDeviceTestAccess::pending(w).is_null() && stable(c, source) == guiCurve,
              "large GUI curve paste previews before writing any source state");
        const auto explanation = AudioDeviceTestAccess::previewText(w);
        check(explanation.contains(juce::String::fromUTF8("跟随小节与拍")) &&
                  explanation.contains("Source performance") && !explanation.contains("source_tempo_hash") &&
                  !explanation.contains("plan_id"),
              "actual GUI preview explains real tracks and musical edits without raw plan JSON");
        AudioDeviceTestAccess::confirm(w, false);
        pump();
        check(stable(c, source) == guiCurve && c.query()["tracks"][0]["clips"].size() == 1,
              "rejecting actual GUI preview preserves curve and clips");
        key(w, 'v');
        AudioDeviceTestAccess::confirm(w, true);
        pump();
        check(c.query()["tracks"][0]["clips"].size() == 2 && stable(c, source) != guiCurve,
              "GUI Paste commits MIDI and mapped automation together");
        c.undo();
        pump();
        check(stable(c, source) == guiCurve && c.query()["tracks"][0]["clips"].size() == 1,
              "GUI musical Paste is one native Undo");
        run(c,
            Json::array({operation("plugin.insert", {{"track", source}, {"type", te::FourOscPlugin::xmlTypeName}}),
                         operation("plugin.insert", {{"track", destination}, {"type", te::FourOscPlugin::xmlTypeName}}),
                         operation("session.range.set", {{"start_samples", 48000}, {"end_samples", 144000}})}));
        c.updateUiState({{"object_selection", Json::array()},
                         {"selection_tracks", Json::array({source})},
                         {"edit_tool", "selector"}},
                        c.sessionToken());
        const auto demo = folder.getChildFile("MusicalCurvesDemo.tracktionedit");
        c.save(demo);
        // Real render of a known PCM signal through the pasted MIDI track's
        // native fader. The test signal is an owned WAV, not a production stub.
        w.openSession(base);
        pump();
        run(c, Json::array({add(source, 0, -18., 1., "$a"), add(source, 96000, -6., 0., "$b"),
                            add(source, 192000, -6., 0., "$e")}));
        buffer = c.prepareMidiRangeClipboard(Json::array({source}), 48000, 144000, c.sessionToken(),
                                             c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        args["clipboard"] = buffer["id"];
        run(c, Json::array({operation("midi.clips.paste", args),
                            operation("session.range.set", {{"start_samples", 288000}, {"end_samples", 480000}})}));
        const auto media = folder.getChildFile("KnownPCM.wav");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = format.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        check(bool(writer), "actual PCM WAV writer created");
        juce::AudioBuffer<float> signal(2, 192000);
        for (int i = 0; i < signal.getNumSamples(); ++i)
        {
            signal.setSample(0, i, .05f);
            signal.setSample(1, i, .025f);
        }
        check(writer->writeFromAudioSampleBuffer(signal, 0, signal.getNumSamples()), "known real stereo PCM written");
        writer.reset();
        const auto hash = Commands::mediaHash(media);
        run(c, Json::array(
                   {operation("plugin.insert", {{"track", destination}, {"type", te::FourOscPlugin::xmlTypeName}})}));
        std::string instrument;
        const auto renderFacts = c.query();
        for (const auto& p : renderFacts["tracks"][1]["plugins"])
            if (p["type"] == te::FourOscPlugin::xmlTypeName)
                instrument = p["id"];
        check(!instrument.empty(), "actual instrument instance found for legal audio/MIDI track render");
        run(c, Json::array({operation("plugin.bypass", {{"plugin", instrument}, {"bypassed", true}})}));
        run(c, Json::array({operation("clip.import", {{"track", destination},
                                                      {"path", media.getFullPathName().toStdString()},
                                                      {"position_samples", 288000},
                                                      {"media_hash", hash}}),
                            operation("track.mute", {{"track", source}, {"enabled", true}})}));
        const auto output = folder.getChildFile("MusicalCurve.wav");
        auto rendered = c.render(output, 0, 576000);
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(output));
        check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == 576000 &&
                  rendered["frames"] == 576000,
              "actual native render header and receipt independently verified");
        juce::AudioBuffer<float> pcm(2, 576000);
        check(reader->read(&pcm, 0, 576000, 0, true, true), "actual rendered PCM decoded");
        double pcmError = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (auto [position, db] : {std::pair{312000, -18.}, {408000, -6.}})
            {
                const double expected = (ch ? .025 : .05) * std::pow(10., db / 20.);
                for (int i = position; i < position + 48000; ++i)
                    pcmError = std::max(pcmError, std::abs(pcm.getSample(ch, i) - expected));
            }
        check(pcmError <= 2e-5, "real fader render follows musically moved step within fixed 2e-5 PCM budget");
        check(Commands::mediaHash(media) == hash, "original owned PCM media unchanged");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"cases", cases},
                    {"maximum_native_error", maximumError},
                    {"frozen_map_error", frozenError},
                    {"zero_boundary_error", zeroError},
                    {"sdk_editor_getter_discrepancy", editorGetterDiscrepancy},
                    {"maximum_pcm_error", pcmError},
                    {"demo", demo.getFullPathName().toStdString()},
                    {"native_error_budget", "1e-7 parameter span plus two float ULP; unchanged"},
                    {"gui", "Mac locked, physical GUI and listening not executed"}};
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
