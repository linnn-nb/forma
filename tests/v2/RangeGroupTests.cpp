#include "Workspace.h"
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
template <class F> void fails(F f, const char* why)
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        failed = true;
    }
    check(failed, why);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(95);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma clipboard tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (!c.isVisible())
        return nullptr;
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
void click(Workspace& w, const juce::String& id)
{
    auto* b = dynamic_cast<juce::TextButton*>(find(w, id));
    if (!b)
        throw std::runtime_error("native button missing: " + id.toStdString());
    b->triggerClick();
    pump();
}
void key(Workspace& w, char k, bool alt = false)
{
    check(w.keyPressed(
              juce::KeyPress(k, juce::ModifierKeys::commandModifier | (alt ? juce::ModifierKeys::altModifier : 0), k)),
          "native global clipboard shortcut accepted");
}
Json run(Commands& c, Json ops)
{
    return c.commit(c.makePlan("human", std::move(ops)));
}
Json clip(Commands& c, int t, int i = 0)
{
    return c.query()["tracks"][t]["clips"][i];
}
void objects(Workspace& w, Commands& c, Json ids)
{
    Json refs = Json::array(), tracks = Json::array();
    const auto facts = c.query();
    for (const auto& t : facts["tracks"])
        for (const auto& q : t["clips"])
            if (std::find(ids.begin(), ids.end(), q["id"]) != ids.end())
            {
                refs.push_back({{"id", q["id"]}, {"track", t["id"]}, {"kind", "clip"}});
                if (std::find(tracks.begin(), tracks.end(), t["id"]) == tracks.end())
                    tracks.push_back(t["id"]);
            }
    c.updateUiState({{"object_selection", refs}, {"selection_tracks", tracks}}, c.sessionToken());
    pump();
}
void range(Workspace& w, Commands& c, int64_t first, int64_t last, Json tracks)
{
    run(c, Json::array({operation("session.range.set", {{"start_samples", first}, {"end_samples", last}})}));
    c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", tracks}}, c.sessionToken());
    pump();
}
void cursor(Workspace& w, Commands& c, int64_t at, const std::string& track)
{
    click(w, "track.select:" + text(track));
    if (!c.timelineRange().is_null())
        run(c, Json::array({operation("session.range.clear", Json::object())}));
    c.seek(at);
    pump();
}
juce::AudioBuffer<float> rendered(Commands& c, const juce::File& f, int64_t start, int64_t end)
{
    auto receipt = c.render(f, start, end);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(f));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == end - start &&
              receipt["frames"] == end - start,
          "real render receipt agrees with independently decoded WAV");
    juce::AudioBuffer<float> pcm(2, int(end - start));
    check(reader->read(&pcm, 0, pcm.getNumSamples(), 0, true, true), "actual rendered PCM decoded");
    return pcm;
}
void sameAudio(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double error = 0, energy = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
        {
            error = std::max(error, std::abs(double(a.getSample(ch, i)) - b.getSample(ch, i)));
            energy += double(a.getSample(ch, i)) * a.getSample(ch, i);
        }
    check(a.getNumSamples() == b.getNumSamples() && energy > 1 && error < 0.00002,
          "frozen gain/fades/EQ and cross-track offsets preserve actual non-silent rendered audio");
}
Json stateOnly(Json tracks)
{
    // Revision and observed automation values are not the saved object state.
    return tracks;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                      .getChildFile("forma-range-" + juce::Uuid().toString());
    if (argc > 2)
        folder = juce::File(argv[2]); // Optional owned, persistent desktop demonstration fixture.
    folder.createDirectory();
    try
    {
        Json sources = Json::array();
        for (int rate : {48000, 44100})
        {
            auto media = folder.getChildFile("source-" + juce::String(rate) + ".wav");
            check(!media.existsAsFile(), "fixture never overwrites existing media");
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
            auto writer = format.createWriterFor(
                stream, juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
            check(bool(writer), "real stereo PCM writer created");
            juce::AudioBuffer<float> pcm(2, rate * 4);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < pcm.getNumSamples(); ++i)
                    pcm.setSample(
                        ch, i,
                        float(.06 * std::sin(2 * juce::MathConstants<double>::pi *
                                             (rate == 48000 ? (ch ? 431 : 997) : (ch ? 659 : 823)) * i / rate)));
            check(writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "actual diagnostic audio written");
            writer.reset();
            sources.push_back({{"path", media.getFullPathName().toStdString()}, {"hash", Commands::mediaHash(media)}});
        }
        Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
        w.setSize(1800, 1600);
        w.setVisible(true);
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c, Json::array(
                   {operation("track.create", {{"name", "Range A"}, {"ref", "$a"}}),
                    operation("track.create", {{"name", "Range B"}, {"ref", "$b"}}),
                    operation("track.create", {{"name", "Unlinked C"}, {"ref", "$c"}}),
                    operation("clip.import",
                              {{"track", "$a"}, {"path", sources[0]["path"]}, {"position_samples", 0}, {"ref", "$ca"}}),
                    operation(
                        "clip.import",
                        {{"track", "$b"}, {"path", sources[1]["path"]}, {"position_samples", 24000}, {"ref", "$cb"}}),
                    operation("clip.import", {{"track", "$c"}, {"path", sources[0]["path"]}, {"position_samples", 0}}),
                    operation("clip.trim", {{"clip", "$ca"}, {"start_samples", 4800}, {"end_samples", 180000}}),
                    operation("clip.trim", {{"clip", "$cb"}, {"start_samples", 36000}, {"end_samples", 180000}}),
                    operation("track.gain", {{"track", "$a"}, {"db", 0.}}),
                    operation("track.gain", {{"track", "$b"}, {"db", 0.}}),
                    operation("clip.gain", {{"clip", "$ca"}, {"db", 0.}}),
                    operation("clip.gain", {{"clip", "$cb"}, {"db", 0.}})}));
        const auto created = c.query();
        const std::string a = created["tracks"][0]["id"], b = created["tracks"][1]["id"];
        const std::string ca = clip(c, 0)["id"], cb = clip(c, 1)["id"];
        run(c, Json::array({operation("track.mute", {{"track", c.query()["tracks"][2]["id"]}, {"enabled", true}})}));
        run(c, Json::array({operation("group.create", {{"id", "range-linked"},
                                                       {"name", "Range linked"},
                                                       {"members", Json::array({a, b})},
                                                       {"enabled", true},
                                                       {"edit", true},
                                                       {"mute", false},
                                                       {"solo", false}})}));
        auto project = folder.getChildFile("RangeDemo.tracktionedit");
        c.save(project);
        auto xml = juce::XmlDocument::parse(project);
        check(bool(xml), "saved native session independently parsed");
        bool found = false;
        std::function<void(juce::XmlElement&)> fractional = [&](juce::XmlElement& node)
        {
            if (node.hasTagName("AUDIOCLIP") && node.getStringAttribute("id") == text(cb))
            {
                node.setAttribute("offset", node.getDoubleAttribute("offset") + .25 / 48000.);
                found = true;
            }
            for (auto* child : node.getChildIterator())
                fractional(*child);
        };
        fractional(*xml);
        check(found, "owned 44.1k source receives fractional project-sample offset");
        auto fractionalFile = folder.getChildFile("Fractional.tracktionedit");
        check(xml->writeTo(fractionalFile), "fractional fixture written to a separate file");
        w.openSession(fractionalFile);
        pump();
        const double offset = clip(c, 1)["source_offset_seconds"];
        const auto original = c.query()["tracks"];
        auto baseline = rendered(c, folder.getChildFile("baseline.wav"), 48000, 132000);
        range(w, c, 60000, 120000, Json::array({a})); // A-only UI seed resolves enabled B in L1.
        const auto before = c.query();
        key(w, 'e');
        auto separated = c.query();
        check(separated["tracks"][0]["clips"].size() == 3 && separated["tracks"][1]["clips"].size() == 3 &&
                  separated["tracks"][2] == before["tracks"][2],
              "CmdE separates both group tracks at both range edges only");
        for (int t : {0, 1})
        {
            check(separated["tracks"][t]["clips"][0]["length_samples"] == (t == 0 ? 55200 : 24000) &&
                      separated["tracks"][t]["clips"][1]["start_samples"] == 60000 &&
                      separated["tracks"][t]["clips"][1]["length_samples"] == 60000 &&
                      separated["tracks"][t]["clips"][2]["start_samples"] == 120000,
                  "separation retains distinct outside starts and common exact slice bounds");
        }
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == original, "one Undo restores both original IDs and topology");
        w.uiCommands().invokeDirectly(7, false);
        check(c.query()["tracks"] == separated["tracks"], "Redo restores precise separated IDs");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        key(w, 'c');
        check(c.clipboard()["tracks"] == Json::array({a, b}) && c.clipboard()["entries"].size() == 2 &&
                  c.query()["tracks"] == original,
              "range Copy resolves full group layout without editing history");
        key(w, 'x');
        const auto cut = c.query();
        check(cut["tracks"][0]["clips"].size() == 2 && cut["tracks"][1]["clips"].size() == 2 &&
                  cut["tracks"][0]["clips"][1]["start_samples"] == 120000 &&
                  cut["tracks"][1]["clips"][1]["start_samples"] == 120000,
              "range Cut removes only common interval and retains both outside tails");
        auto gap = rendered(c, folder.getChildFile("gap.wav"), 60000, 120000);
        check(gap.getMagnitude(0, 2048, gap.getNumSamples() - 4096) < 2e-5 &&
                  gap.getMagnitude(1, 2048, gap.getNumSamples() - 4096) < 2e-5,
              "real Tracktion render proves Cut gap silent");
        key(w, 'v', true);
        const auto pasted = c.query();
        auto findAt = [](const Json& clips, int64_t at)
        {
            for (const auto& item : clips)
                if (item["start_samples"] == at)
                    return item;
            throw std::runtime_error("expected slice missing");
        };
        const auto middleB = findAt(pasted["tracks"][1]["clips"], 60000);
        check(std::abs(middleB["source_offset_seconds"].get<double>() - offset - .5) < 1e-12,
              "frozen clipboard preserves fractional source seconds at 44.1k");
        auto restored = rendered(c, folder.getChildFile("restored.wav"), 48000, 132000);
        double pcmError = 0;
        // Native graph has edge ramps: compare actual samples away from newly introduced boundaries.
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 14048; i < 69952; ++i)
                pcmError = std::max(pcmError, std::abs(double(restored.getSample(ch, i)) - baseline.getSample(ch, i)));
        std::cout << "PCM error=" << pcmError << " peak=" << restored.getMagnitude(0, 14048, 55904) << std::endl;
        check(pcmError < 2e-5 && restored.getMagnitude(0, 14048, 55904) > .01,
              "actual non-silent restored PCM matches original in declared slice interior");
        w.uiCommands().invokeDirectly(6, false);
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == original, "Cut and Paste each Undo as a single native transaction");
        pump();
        const auto noImplicitSlice = c.audioRangeOperations("move", Json::array({a}), 60000, 120000, 480);
        check(noImplicitSlice.empty(), "default range Nudge skips partial clips instead of silently separating them");
        key(w, 'e');
        const auto beforeNudge = c.query();
        pump();
        check(w.keyPressed(juce::KeyPress('.', 0, '.')), "global range Nudge shortcut accepted");
        const auto moved = c.query();
        check(findAt(moved["tracks"][0]["clips"], 60480)["id"] ==
                      findAt(beforeNudge["tracks"][0]["clips"], 60000)["id"] &&
                  findAt(moved["tracks"][1]["clips"], 60480)["id"] ==
                      findAt(beforeNudge["tracks"][1]["clips"], 60000)["id"],
              "range Nudge preserves both stable middle clip identities");
        check(findAt(moved["tracks"][0]["clips"], 60480)["length_samples"] == 60000 &&
                  findAt(moved["tracks"][1]["clips"], 60480)["length_samples"] == 60000 &&
                  moved["time_selection"]["start_samples"] == 60480 &&
                  moved["time_selection"]["end_samples"] == 120480 && moved["tracks"][2] == before["tracks"][2],
              "Nudge moves only isolated group slices and selection by 480 samples");
        check(findAt(moved["tracks"][0]["clips"], 4800)["length_samples"] == 55200 &&
                  findAt(moved["tracks"][1]["clips"], 36000)["length_samples"] == 24000 &&
                  findAt(moved["tracks"][1]["clips"], 120000)["length_samples"] == 60000,
              "Nudge leaves all outside tails in place rather than moving whole group clips");
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == beforeNudge["tracks"] && c.timelineRange() == beforeNudge["time_selection"],
              "range Nudge Undo restores separated clip IDs and range together");
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == original, "prior Separate remains an independent Undo transaction");
        pump();
        check(w.uiCommands().invokeDirectly(editCommand::remove, false), "global range Delete command accepted");
        check(c.query()["tracks"][0]["clips"].size() == 2 && c.query()["tracks"][1]["clips"].size() == 2,
              "Delete performs precise range removal on current enabled group");
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == original, "range Delete restores original state with one Undo");
        const auto leftMove = c.audioRangeOperations("move", Json::array({a}), 0, 60000, 480);
        check(leftMove.empty() && c.query()["tracks"] == original,
              "start-covering partial Nudge leaves unsplit clips unchanged");
        fails([&] { c.audioRangeOperations("move", Json::array({a}), 36000, 190000, 480); },
              "mixed whole/partial group range Nudge requires explicit Separate instead of moving outside audio");
        check(c.query()["tracks"] == original, "ambiguous group Nudge leaves every clip identity and position intact");
        run(c, c.audioRangeOperations("move", Json::array({a}), 0, 190000, 480));
        check(clip(c, 0)["start_samples"] == 5280 && clip(c, 1)["start_samples"] == 36480 && clip(c, 0)["id"] == ca &&
                  clip(c, 1)["id"] == cb,
              "fully-contained grouped clips are nudged once without replacing IDs");
        c.undo();
        run(c, c.audioRangeOperations("separate", Json::array({a}), 60001, 60002));
        check(clip(c, 0, 1)["length_samples"] == 1 && clip(c, 1, 1)["length_samples"] == 1,
              "real split primitive keeps one-project-sample slices on both source rates");
        c.undo();
        check(c.query()["tracks"] == original, "one-sample Separate restores exact originals with Undo");
        auto stale = c.makePlan("human", c.audioRangeOperations("delete", Json::array({a}), 60000, 120000));
        run(c, Json::array({operation("clip.lock", {{"clip", cb}, {"locked", true}})}));
        const auto locked = c.query();
        fails([&] { c.commit(stale); }, "stale range Plan cannot override interleaved human lock");
        fails([&] { c.audioRangeOperations("delete", Json::array({a}), 60000, 120000); },
              "locked overlapping peer rejects entire range edit");
        check(c.query()["tracks"] == locked["tracks"] && c.query()["revision"] == locked["revision"],
              "rejected range edits leave project and revision unchanged");
        c.undo();
        fails([&] { c.audioRangeOperations("move", Json::array({a}), 100, 120000, -480); },
              "negative range Nudge rejects before any native slicing");
        pump();
        range(w, c, 0, 190000, Json::array({a}));
        check(!w.keyPressed(juce::KeyPress('e', juce::ModifierKeys::commandModifier, 'e')),
              "Separate shortcut is disabled without interior boundaries");
        check(c.query()["tracks"] == original, "range Separate already at outside edges is disabled/no-op");
        range(w, c, 0, 61 * 48000, Json::array({a}));
        const auto beforeLarge = c.query();
        check(w.uiCommands().invokeDirectly(editCommand::remove, false), "large range Delete enters preview");
        check(find(w, "plan.accept") && find(w, "plan.reject") && c.query()["tracks"] == original &&
                  c.query()["revision"] == beforeLarge["revision"],
              "large destructive range shows actual preview without mutating Edit");
        click(w, "plan.reject");
        check(!find(w, "plan.accept") && c.query()["tracks"] == original, "rejecting range preview preserves project");
        check(w.uiCommands().invokeDirectly(editCommand::remove, false), "large range Delete can be proposed again");
        click(w, "plan.accept");
        check(c.query()["tracks"][0]["clips"].empty() && c.query()["tracks"][1]["clips"].empty(),
              "accepted range preview deletes actual grouped clips");
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == original, "accepted preview is one native Undo transaction");
        Json dense = Json::array();
        for (int i = 0; i < 30; ++i)
            dense.push_back(operation(
                "clip.copy",
                {{"clip", ca}, {"track", a}, {"position_samples", 60000}, {"ref", "$dense-" + std::to_string(i)}}));
        run(c, dense);
        const auto beforeBudget = c.query();
        fails([&] { c.audioRangeOperations("delete", Json::array({a}), 60000, 120000); },
              "over-budget range rejects the entire edit before any native mutation");
        check(c.query()["tracks"] == beforeBudget["tracks"] && c.query()["revision"] == beforeBudget["revision"],
              "64-operation budget refusal preserves all dense clips");
        c.undo();
        check(c.query()["tracks"] == original, "owned dense fixture Undo restores original group");
        // A one-track clipboard may not destructively replace half an enabled destination group.
        auto capture =
            c.prepareClipboard(Json::array({{{"clip", ca}, {"start_samples", 60000}, {"end_samples", 120000}}}),
                               Json::array({a}), 60000, 120000, c.sessionToken(), c.query()["revision"]);
        c.acceptClipboard(capture["id"]);
        cursor(w, c, 200000, a);
        const auto mismatch = c.query();
        key(w, 'v');
        check(c.query()["tracks"] == mismatch["tracks"] && c.query()["revision"] == mismatch["revision"],
              "mismatched clipboard destination group rejects without erasing a peer");
        range(w, c, 60000, 120000, Json::array({a}));
        key(w, 'e');
        auto saved = c.query();
        project = folder.getChildFile("EditedRange.tracktionedit");
        c.save(project);
        Workspace reopened(false, std::make_unique<Storage>(folder.getChildFile("reopen-prefs")));
        reopened.setSize(1800, 1600);
        reopened.openSession(project);
        pump();
        auto& opened = AudioDeviceTestAccess::owner(reopened);
        const auto reopenedFacts = opened.query();
        auto reopenedTracks = reopenedFacts["tracks"];
        double reopenSourceError = 0;
        // Native XML double round trips can differ at the last binary digit. Apply only the source-seconds
        // tolerance declared before this test; integer sample positions, IDs, topology and other state stay exact.
        for (const auto& difference : Json::diff(saved["tracks"], reopenedTracks))
        {
            const auto path = difference["path"].get<std::string>();
            if (difference["op"] == "replace" && path.ends_with("/source_offset_seconds"))
            {
                const Json::json_pointer pointer(path);
                const auto error =
                    std::abs(saved["tracks"].at(pointer).get<double>() - reopenedTracks.at(pointer).get<double>());
                reopenSourceError = std::max(reopenSourceError, error);
                if (error <= 1e-12)
                    reopenedTracks[pointer] = saved["tracks"].at(pointer);
            }
        }
        check(
            reopenedTracks == saved["tracks"] && opened.timelineRange() == saved["time_selection"] &&
                reopenedFacts["mix_groups"] == saved["mix_groups"],
            "native reopen retains exact IDs topology groups sample bounds and source time within declared tolerance");
        const auto custom = juce::KeyPress(
            'e', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            'e');
        w.uiCommands().getKeyMappings()->clearAllKeyPresses(editCommand::split);
        w.uiCommands().getKeyMappings()->addKeyPress(editCommand::split, custom);
        pump();
        check(w.uiCommands().getKeyMappings()->findCommandForKeyPress(custom) == editCommand::split,
              "custom Separate mapping is unique in the actual registry");
        project = folder.getChildFile("CustomKeys.tracktionedit");
        c.save(project);
        reopened.openSession(project);
        pump();
        check(reopened.uiCommands().getKeyMappings()->containsMapping(editCommand::split, custom) &&
                  reopened.uiCommands().getKeyMappings()->findCommandForKeyPress(custom) == editCommand::split,
              "range Separate uses persisted custom global command mapping");
        c.undo();
        pump();
        check(w.uiCommands().getKeyMappings()->keyPressed(custom, &w), "custom range Separate shortcut dispatches");
        pump();
        check(c.query()["tracks"][0]["clips"].size() == 3 && c.query()["tracks"][1]["clips"].size() == 3,
              "custom shortcut actually separates current group range");
        c.undo();
        c.updateUiState({{"span_samples", 192000},
                         {"edit_tool", "selector"},
                         {"edit_mode", "slip"},
                         {"selection_tracks", Json::array({a, b})}},
                        c.sessionToken());
        c.seek(60000);
        project = folder.getChildFile("RangeDemoReady.tracktionedit");
        c.save(project); // Leave the owned demonstration at an editable, unseparated range.
        for (const auto& source : sources)
            check(Commands::mediaHash(juce::File(text(source["path"]))) == source["hash"].get<std::string>(),
                  "every edit and Undo preserves original diagnostic media bytes");
        Json report = {
            {"result", "passed"},
            {"checks", checks},
            {"pcm_max_error", pcmError},
            {"reopen_source_offset_max_error_seconds", reopenSourceError},
            {"budgets", {{"pcm_error", 2e-5}, {"fractional_offset_seconds", 1e-12}, {"timeout_seconds", 120}}},
            {"demo", project.getFullPathName().toStdString()},
            {"scope", "actual native commands, Edit group range slicing, frozen clipboard, Undo/Redo, save/reopen, "
                      "decoded Tracktion WAV; physical GUI/listening not verified"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        if (argc <= 2)
            folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
