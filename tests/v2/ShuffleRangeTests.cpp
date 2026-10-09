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
    static Json pending(Workspace& w)
    {
        return w.pending;
    }
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
void range(Workspace& w, Commands& c, int64_t first, int64_t last, Json tracks)
{
    run(c, Json::array({operation("session.range.set", {{"start_samples", first}, {"end_samples", last}})}));
    c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", tracks}}, c.sessionToken());
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
} // namespace
Json orderedClips(const Json& owner)
{
    auto clips = owner["clips"];
    std::sort(clips.begin(), clips.end(), [](const Json& a, const Json& b)
              { return a["start_samples"].get<int64_t>() < b["start_samples"].get<int64_t>(); });
    return clips;
}
Json byID(const Json& tracks, const std::string& id)
{
    for (const auto& t : tracks)
        for (const auto& clip : t["clips"])
            if (clip["id"] == id)
                return clip;
    throw std::runtime_error("missing stable test clip");
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto folder = parent.getChildFile("shuffle-" + juce::Uuid().toString());
    folder.createDirectory();
    double pcmError = 0, sourceTimeError = 0;
    try
    {
        Json sources = Json::array();
        for (int rate : {48000, 44100})
        {
            const auto media = folder.getChildFile("source-" + juce::String(rate) + ".wav");
            check(!media.exists(), "unique owned fixture preserves existing files");
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
            auto writer = format.createWriterFor(
                stream, juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
            check(bool(writer), "real stereo PCM writer created");
            juce::AudioBuffer<float> audio(2, rate * 6);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < audio.getNumSamples(); ++i)
                    audio.setSample(
                        ch, i,
                        float(.08 * std::sin(2 * juce::MathConstants<double>::pi *
                                             (rate == 48000 ? (ch ? 431 : 997) : (ch ? 659 : 823)) * i / rate)));
            check(writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()), "actual diagnostic PCM written");
            writer.reset();
            sources.push_back({{"path", media.getFullPathName().toStdString()}, {"hash", Commands::mediaHash(media)}});
        }
        Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
        w.setSize(1800, 1400);
        w.setVisible(true);
        pump();
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c, Json::array(
                   {operation("track.create", {{"name", "Shuffle A"}, {"ref", "$a"}}),
                    operation("track.create", {{"name", "Shuffle B"}, {"ref", "$b"}}),
                    operation("track.create", {{"name", "Unlinked"}, {"ref", "$c"}}),
                    operation("clip.import",
                              {{"track", "$a"}, {"path", sources[0]["path"]}, {"position_samples", 0}, {"ref", "$a0"}}),
                    operation("clip.trim", {{"clip", "$a0"}, {"start_samples", 0}, {"end_samples", 150000}}),
                    operation(
                        "clip.import",
                        {{"track", "$a"}, {"path", sources[0]["path"]}, {"position_samples", 180000}, {"ref", "$a1"}}),
                    operation("clip.trim", {{"clip", "$a1"}, {"start_samples", 180000}, {"end_samples", 220000}}),
                    operation(
                        "clip.import",
                        {{"track", "$b"}, {"path", sources[0]["path"]}, {"position_samples", 36000}, {"ref", "$b0"}}),
                    operation("clip.trim", {{"clip", "$b0"}, {"start_samples", 36000}, {"end_samples", 240000}}),
                    operation(
                        "clip.import",
                        {{"track", "$b"}, {"path", sources[0]["path"]}, {"position_samples", 300000}, {"ref", "$b1"}}),
                    operation("clip.trim", {{"clip", "$b1"}, {"start_samples", 300000}, {"end_samples", 340000}}),
                    operation("clip.import", {{"track", "$c"}, {"path", sources[0]["path"]}, {"position_samples", 0}}),
                    operation("track.mute", {{"track", "$c"}, {"enabled", true}})}));
        const auto tracks = c.query()["tracks"];
        const std::string a = tracks[0]["id"], b = tracks[1]["id"];
        const std::string a0 = tracks[0]["clips"][0]["id"], a1 = tracks[0]["clips"][1]["id"];
        const std::string b0 = tracks[1]["clips"][0]["id"], b1 = tracks[1]["clips"][1]["id"];
        run(c, Json::array({operation("group.create", {{"id", "shuffle-linked"},
                                                       {"name", "Phase pair"},
                                                       {"members", Json::array({a, b})},
                                                       {"enabled", true},
                                                       {"edit", true},
                                                       {"mute", false},
                                                       {"solo", false}})}));
        const auto saved = folder.getChildFile("Original.tracktionedit");
        c.save(saved);
        auto xml = juce::XmlDocument::parse(saved);
        bool changed = false;
        std::function<void(juce::XmlElement&)> fractional = [&](juce::XmlElement& n)
        {
            if (n.hasTagName("AUDIOCLIP") && n.getStringAttribute("id") == text(b0))
            {
                n.setAttribute("offset", n.getDoubleAttribute("offset") + .25 / 48000.);
                changed = true;
            }
            for (auto* child : n.getChildIterator())
                fractional(*child);
        };
        check(bool(xml), "real session XML parsed");
        fractional(*xml);
        const auto fractionFile = folder.getChildFile("Fractional.tracktionedit");
        check(changed && xml->writeTo(fractionFile), "fractional source time fixture uses separate owned session");
        w.openSession(fractionFile);
        pump();
        const auto original = c.query()["tracks"];
        const double bOffset = original[1]["clips"][0]["source_offset_seconds"];
        const auto beforeAudio = rendered(c, folder.getChildFile("before.wav"), 0, 400000);
        range(w, c, 48000, 96000, Json::array({a}));
        run(c, Json::array({operation("session.insertion.set", {{"position_samples", 17000}})}));
        pump();
        const auto before = c.query();
        const auto plan = c.makeShuffleRangePlan(Json::array({a}), 48000, 96000);
        const auto diff = c.preview(plan);
        check(diff["clip_changes"].size() >= 8 && c.query()["tracks"] == original,
              "preview includes exact splits deletes and later moves without mutating Edit");
        for (int kind = 0; kind < 5; ++kind)
        {
            auto bad = plan;
            if (kind == 0)
                bad["operations"].erase(bad["operations"].begin());
            if (kind == 1)
                bad["shuffle_range"]["tracks"] = Json::array({b});
            if (kind == 2)
                bad["shuffle_range"]["end_samples"] = 96001;
            if (kind == 3)
                bad["requested_operations"] = bad["operations"];
            if (kind == 4)
                bad["operations"][0]["args"]["media_hash"] = "stale";
            // Different seed within the SAME group has identical semantics; change to the unlinked owner instead.
            if (kind == 1)
                bad["shuffle_range"]["tracks"] = Json::array({original[2]["id"]});
            fails([&] { c.commit(bad); }, "tampered range descriptor/primitive list cannot bypass group validation");
        }
        auto alien = plan;
        alien["actor"] = "agent:test";
        fails([&] { c.commit(alien, true); }, "range compiler is local human only; no frozen MCP tool expansion");
        Scope readOnly;
        readOnly.mode = Permission::ReadOnly;
        fails([&] { c.commit(plan, true, readOnly); }, "read-only scope denies actual expanded range transaction");
        Scope bounded;
        bounded.targets = {a};
        fails([&] { c.commit(plan, true, bounded); }, "scope includes every linked track and refuses ungranted peer");
        auto receipt = c.commit(plan);
        const auto after = c.query();
        check(receipt["state"] == "committed" &&
                  after["revision"].get<uint64_t>() == before["revision"].get<uint64_t>() + 1,
              "whole range and group commit once with authoritative receipt");
        for (int t : {0, 1})
        {
            const auto clips = orderedClips(after["tracks"][t]);
            check(clips.size() == 3 && clips[0]["id"] == (t == 0 ? a0 : b0) &&
                      clips[0]["start_samples"] == (t == 0 ? 0 : 36000) &&
                      clips[0]["length_samples"] == (t == 0 ? 48000 : 12000),
                  "retained group prefixes keep identity and original position");
            check(clips[1]["start_samples"] == 48000 && clips[1]["length_samples"] == (t == 0 ? 54000 : 144000),
                  "group right tails collapse to exact selection start from distinct boundaries");
            check(clips[2]["id"] == (t == 0 ? a1 : b1) && clips[2]["start_samples"] == (t == 0 ? 132000 : 252000),
                  "later clips retain stable IDs and original inter-clip gaps");
        }
        check(std::abs(orderedClips(after["tracks"][1])[1]["source_offset_seconds"].get<double>() -
                       (bOffset + 60000. / 48000.)) < 1e-12,
              "fractional source-time mapping survives split and time collapse");
        check(after["tracks"][2] == original[2] && after["time_selection"].is_null() &&
                  after["position_samples"] == 48000,
              "unlinked track untouched and range/cursor collapse in same transaction");
        const auto afterAudio = rendered(c, folder.getChildFile("after.wav"), 0, 352000);
        double energy = 0;
        int errorIndex = 0, errorChannel = 0;
        // Predeclared 2048-frame exclusion only around native split/resampler boundaries.
        // Every other sample is compared to the independently rendered pre-edit time splice.
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 352000; ++i)
            {
                if (std::abs(i - 48000) < 2048)
                    continue;
                const auto expected = beforeAudio.getSample(ch, i < 48000 ? i : i + 48000);
                const double error = std::abs(double(afterAudio.getSample(ch, i)) - expected);
                if (error > pcmError)
                {
                    pcmError = error;
                    errorIndex = i;
                    errorChannel = ch;
                }
                energy += expected * expected;
            }
        std::cout << "PCM error " << pcmError << " at " << errorIndex << " ch " << errorChannel << " expected "
                  << beforeAudio.getSample(errorChannel, errorIndex < 48000 ? errorIndex : errorIndex + 48000)
                  << " actual " << afterAudio.getSample(errorChannel, errorIndex) << std::endl;
        check(energy > 1 && pcmError < 2e-5,
              "real stereo mix equals pre-edit PCM time splice within declared tolerance");
        const auto replay = c.commit(plan);
        check(replay["replayed"] == true && c.query()["tracks"] == after["tracks"],
              "retry returns receipt without a second time collapse");
        c.undo();
        pump();
        check(c.query()["tracks"] == original && c.timelineRange() == before["time_selection"] &&
                  c.query()["position_samples"] == 17000,
              "one Undo restores exact grouped media range and insertion");
        c.redo();
        pump();
        check(c.query()["tracks"] == after["tracks"], "one Redo restores right-tail identities and sample positions");
        c.undo();
        pump();
        // Boundary-only gap: no selected audio, but subsequent clips must still collapse.
        c.updateUiState({{"edit_mode", "shuffle"}}, c.sessionToken());
        pump();
        range(w, c, 260000, 280000, Json::array({b}));
        const auto gapBefore = c.query();
        juce::ApplicationCommandInfo info(editCommand::remove);
        w.getCommandInfo(editCommand::remove, info);
        check(!(info.flags & juce::ApplicationCommandInfo::isDisabled),
              "Delete remains enabled for a selected empty gap with later audio");
        check(w.uiCommands().invokeDirectly(editCommand::remove, true),
              "native Delete command dispatches empty-gap Shuffle");
        pump();
        check(c.query()["tracks"][1]["clips"][1]["start_samples"] == 280000 && c.query()["tracks"][0] == original[0],
              "empty-gap Delete shifts only later clips; linked earlier audio unchanged");
        c.undo();
        pump();
        check(c.query()["tracks"] == gapBefore["tracks"] && c.timelineRange() == gapBefore["time_selection"],
              "empty-gap Delete is one reversible native transaction");
        // Every failure leaves all earlier planned splits and moves unapplied.
        run(c, Json::array({operation("clip.lock", {{"clip", b1}, {"locked", true}})}));
        const auto locked = c.query();
        fails([&] { c.makeShuffleRangePlan(Json::array({a}), 48000, 96000); },
              "locked later group clip rejects entire collapse before any split");
        check(c.query()["tracks"] == locked["tracks"], "locked-tail rejection preserves actual state");
        c.undo();
        pump();
        auto stale = c.makeShuffleRangePlan(Json::array({a}), 48000, 96000);
        run(c, Json::array({operation("track.comment", {{"track", a}, {"value", "manual edit during preview"}})}));
        fails([&] { c.commit(stale); }, "manual edit during planning invalidates range Plan");
        c.undo();
        pump();
        for (const auto& bounds :
             Json::array({Json::array({-1, 100}), Json::array({100, 100}), Json::array({100, 99})}))
            fails([&] { c.makeShuffleRangePlan(Json::array({a}), bounds[0], bounds[1]); },
                  "invalid time range rejected before allocation of Edit pieces");
        fails([&] { c.makeShuffleRangePlan(Json::array({a, a}), 48000, 96000); }, "duplicate seed tracks rejected");
        fails([&] { c.makeShuffleRangePlan(Json::array({"missing"}), 48000, 96000); }, "stale track identity rejected");
        // Native GUI Cut and clipboard use the SAME whole-range compiler.
        c.updateUiState({{"edit_mode", "shuffle"}}, c.sessionToken());
        pump();
        range(w, c, 48000, 96000, Json::array({a}));
        const auto cutBefore = c.query();
        key(w, 'x');
        const auto cutAfter = c.query();
        const auto clipboard = c.clipboard();
        check(byID(cutAfter["tracks"], a1)["start_samples"] == 132000 && clipboard["entries"].size() == 2 &&
                  clipboard["start_samples"] == 48000 && clipboard["end_samples"] == 96000,
              "CmdX actually cuts the grouped partial range and freezes exact clipboard slices");
        c.undo();
        pump();
        check(c.query()["tracks"] == cutBefore["tracks"] && c.timelineRange() == cutBefore["time_selection"],
              "one GUI Cut Undo restores Edit and keeps copied audio available");
        c.updateUiState({{"edit_mode", "slip"}}, c.sessionToken());
        pump();
        key(w, 'v', true); // Paste at original timestamp is replacement, not Shuffle insertion (documented boundary).
        check(c.query()["revision"].get<uint64_t>() > cutAfter["revision"].get<uint64_t>(),
              "frozen cut snapshots paste through native original-timestamp shortcut");
        const auto pasted = rendered(c, folder.getChildFile("pasted.wav"), 0, 400000);
        double pasteError = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 2048; i < 398000; ++i)
                if (std::abs(i - 48000) > 2048 && std::abs(i - 96000) > 2048)
                    pasteError =
                        std::max(pasteError, std::abs(double(pasted.getSample(ch, i)) - beforeAudio.getSample(ch, i)));
        check(pasteError < 2e-5,
              "Cut Undo and frozen Paste Original reproduce actual source mix away from split edges");
        c.undo();
        pump();
        const auto custom = juce::KeyPress(
            'd', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            'd');
        w.uiCommands().getKeyMappings()->clearAllKeyPresses(editCommand::remove);
        w.uiCommands().getKeyMappings()->addKeyPress(editCommand::remove, custom);
        pump();
        c.updateUiState({{"edit_mode", "shuffle"}, {"span_samples", 400000}, {"edit_tool", "selector"}},
                        c.sessionToken());
        pump();
        range(w, c, 48000, 96000, Json::array({a}));
        const auto ready = folder.getChildFile("ShuffleReady.tracktionedit");
        c.save(ready);
        w.openSession(ready);
        pump();
        check(w.uiCommands().getKeyMappings()->containsMapping(editCommand::remove, custom),
              "custom Delete shortcut survives native save/reopen");
        check(w.uiCommands().getKeyMappings()->keyPressed(custom, &w),
              "custom Delete shortcut dispatches actual grouped range edit");
        pump();
        check(byID(c.query()["tracks"], a1)["start_samples"] == 132000 && c.timelineRange().is_null(),
              "reopened custom key collapses grouped audio and current selection");
        const auto committed = c.query();
        const auto collapsed = folder.getChildFile("ShuffleCollapsed.tracktionedit");
        c.save(collapsed);
        c.undo();
        pump();
        check(c.query()["tracks"][0]["clips"][1]["id"] == a1, "reopened native edit remains undoable until close");
        w.openSession(collapsed);
        pump();
        auto reopened = c.query()["tracks"];
        for (const auto& delta : Json::diff(committed["tracks"], reopened))
            if (delta["op"] == "replace" && delta["path"].get<std::string>().ends_with("/source_offset_seconds"))
            {
                const Json::json_pointer ptr(delta["path"].get<std::string>());
                sourceTimeError = std::max(sourceTimeError, std::abs(reopened.at(ptr).get<double>() -
                                                                     committed["tracks"].at(ptr).get<double>()));
                if (sourceTimeError < 1e-12)
                    reopened[ptr] = committed["tracks"].at(ptr);
            }
        check(
            reopened == committed["tracks"] && c.timelineRange().is_null() &&
                c.query()["mix_groups"] == committed["mix_groups"],
            "saved collapsed session reopens exact topology identities and groups with declared source-time tolerance");
        // A short destructive selection can affect many later clips; previews count those too.
        w.openSession(ready);
        pump();
        Json added = Json::array();
        for (int i = 0; i < 6; ++i)
        {
            const auto ref = "$extra-" + std::to_string(i);
            const int64_t at = 400000 + i * 100000;
            added.push_back(operation(
                "clip.import", {{"track", a}, {"path", sources[0]["path"]}, {"position_samples", at}, {"ref", ref}}));
            added.push_back(
                operation("clip.trim", {{"clip", ref}, {"start_samples", at}, {"end_samples", at + 48000}}));
        }
        run(c, added);
        pump();
        range(w, c, 48000, 96000, Json::array({a}));
        const auto manyBefore = c.query();
        const auto oldClipboard = c.clipboard();
        check(w.uiCommands().invokeDirectly(editCommand::cut, true),
              "short Shuffle Cut with many later targets dispatches preview");
        pump();
        check(!AudioDeviceTestAccess::pending(w).is_null() && c.query()["tracks"] == manyBefore["tracks"] &&
                  c.clipboard() == oldClipboard,
              "short selection with over eight affected clips previews without changing Edit or clipboard");
        click(w, "plan.reject");
        check(c.query()["tracks"] == manyBefore["tracks"] && c.clipboard() == oldClipboard,
              "rejecting large Shuffle Cut preserves native project and previous clipboard");
        check(w.uiCommands().invokeDirectly(editCommand::cut, true), "large Cut can be proposed after rejection");
        pump();
        click(w, "plan.accept");
        check(byID(c.query()["tracks"], a1)["start_samples"] == 132000 && !c.clipboard().is_null(),
              "accepting actual preview commits collapse and copied slices together");
        c.undo();
        pump();
        check(c.query()["tracks"] == manyBefore["tracks"] && c.timelineRange() == manyBefore["time_selection"],
              "accepted large Cut is one Undo including range/cursor");
        check(w.uiCommands().invokeDirectly(editCommand::remove, true),
              "large Delete proposes whole later-clip impact");
        pump();
        check(!AudioDeviceTestAccess::pending(w).is_null() && c.query()["tracks"] == manyBefore["tracks"],
              "Delete counts later targets when selected overlap count is small");
        click(w, "plan.reject");
        c.undo();
        pump(); // Undo the range selection, then the six added clips.
        c.undo();
        pump();
        w.openSession(ready);
        pump();
        // Linked unsupported MIDI must reject before editing either audio track.
        run(c, Json::array({operation("track.create", {{"name", "Linked MIDI"}, {"type", "midi"}, {"ref", "$m"}}),
                            operation("midi.clip.create", {{"track", "$m"},
                                                           {"name", "Notes"},
                                                           {"ref", "$notes"},
                                                           {"position_samples", 100000},
                                                           {"length_samples", 48000}})}));
        const auto midiID = c.query()["tracks"][3]["id"];
        run(c, Json::array({operation("group.create", {{"id", "midi-linked"},
                                                       {"name", "Unsupported mixed group"},
                                                       {"members", Json::array({b, midiID})},
                                                       {"enabled", true},
                                                       {"edit", true},
                                                       {"mute", false},
                                                       {"solo", false}})}));
        const auto midiBefore = c.query();
        fails([&] { c.makeShuffleRangePlan(Json::array({a}), 48000, 96000); },
              "transitively linked MIDI later in selection rejects entire audio collapse");
        check(c.query()["tracks"] == midiBefore["tracks"], "unsupported later group target leaves all audio intact");
        w.openSession(ready);
        pump();
        run(c, Json::array({operation("clip.import",
                                      {{"track", b}, {"path", sources[1]["path"]}, {"position_samples", 500000}})}));
        const auto mixedBefore = c.query();
        fails([&] { c.makeShuffleRangePlan(Json::array({a}), 48000, 96000); },
              "real 44.1k later audio explicitly refuses Shuffle until native resampler is qualified");
        check(c.query()["tracks"] == mixedBefore["tracks"] && c.query()["revision"] == mixedBefore["revision"],
              "mixed-rate refusal preserves complete 48k/44.1k arrangement and does not resample original media");
        w.openSession(ready);
        pump();
        run(c, Json::array({operation("automation.point.add", {{"track", b},
                                                               {"parameter", "volume"},
                                                               {"ref", "$point"},
                                                               {"position_samples", 160000},
                                                               {"value", -6.},
                                                               {"curve", 0.}})}));
        const auto automationBefore = c.query();
        const auto curves = c.automationQuery(b);
        fails([&] { c.makeShuffleRangePlan(Json::array({a}), 48000, 96000); },
              "linked automation explicitly refuses unimplemented follow-edit semantics");
        check(c.query()["tracks"] == automationBefore["tracks"] && c.automationQuery(b) == curves,
              "automation refusal preserves all curve points and audio");
        w.openSession(ready);
        pump();
        // Actual Edit workload, not a mock: 65 later clips exceed the fixed Plan limit.
        for (int batch = 0; batch < 3; ++batch)
        {
            Json ops = Json::array();
            for (int i = 0; i < (batch == 2 ? 17 : 24); ++i)
            {
                const int n = batch * 24 + i;
                ops.push_back(
                    operation("clip.import",
                              {{"track", a}, {"path", sources[0]["path"]}, {"position_samples", 500000 + n * 400000}}));
            }
            run(c, ops);
        }
        const auto budgetBefore = c.query();
        fails([&] { c.makeShuffleRangePlan(Json::array({a}), 48000, 96000); },
              "65 real later clips exceed fixed budget and reject whole Plan");
        check(c.query()["tracks"] == budgetBefore["tracks"] && c.query()["revision"] == budgetBefore["revision"],
              "budget failure never partially splits or silently drops tracks");
        w.openSession(ready);
        pump();
        for (const auto& source : sources)
            check(Commands::mediaHash(juce::File(text(source["path"]))) == source["hash"].get<std::string>(),
                  "all edits render Undo and reopen preserve original PCM hashes");
        Json report{
            {"result", "passed"},
            {"checks", checks},
            {"pcm_max_error", pcmError},
            {"paste_pcm_max_error", pasteError},
            {"source_time_reopen_error_seconds", sourceTimeError},
            {"demo", ready.getFullPathName().toStdString()},
            {"budgets", {{"pcm_error", 2e-5}, {"split_edge_exclusion_frames", 2048}, {"source_time_seconds", 1e-12}}},
            {"scope", "real native Edit, group Shuffle partial/gap Cut/Delete, strict preview and permissions, native "
                      "Undo, PCM render, GUI keys, save/reopen; physical desktop separate"}};
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
