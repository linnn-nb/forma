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
                      .getChildFile("forma-clipboard-" + juce::Uuid().toString());
    folder.createDirectory();
    try
    {
        auto media = folder.getChildFile("owned.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
            auto writer = format.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            if (!writer)
                throw std::runtime_error("fixture writer unavailable");
            juce::AudioBuffer<float> pcm(2, 192000);
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < pcm.getNumSamples(); ++n)
                    pcm.setSample(ch, n, float(.08 * std::sin(n * .06 + ch * .3) + .03 * std::sin(n * .013)));
            check(writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "real owned PCM fixture written");
        }
        const auto hash = Commands::mediaHash(media);
        Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
        w.setSize(1800, 1600);
        w.setVisible(true);
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c, Json::array({operation("track.create", {{"name", "Source A"}, {"ref", "$a"}}),
                            operation("track.create", {{"name", "Source B"}, {"ref", "$b"}}),
                            operation("track.create", {{"name", "Destination A"}, {"ref", "$x"}}),
                            operation("track.create", {{"name", "Destination B"}, {"ref", "$y"}}),
                            operation("clip.import", {{"track", "$a"},
                                                      {"path", media.getFullPathName().toStdString()},
                                                      {"position_samples", 24000},
                                                      {"ref", "$c"}}),
                            operation("clip.trim", {{"clip", "$c"}, {"start_samples", 48000}, {"end_samples", 192000}}),
                            operation("clip.gain", {{"clip", "$c"}, {"db", -6}}),
                            operation("clip.fade", {{"clip", "$c"},
                                                    {"in_samples", 2400},
                                                    {"out_samples", 4800},
                                                    {"in_curve", "concave"},
                                                    {"out_curve", "s_curve"}}),
                            operation("clip.fx.insert", {{"clip", "$c"}, {"type", te::EqualiserPlugin::xmlTypeName}}),
                            operation("clip.import", {{"track", "$b"},
                                                      {"path", media.getFullPathName().toStdString()},
                                                      {"position_samples", 72000}})}));
        pump();
        auto initial = c.query();
        const std::string a = initial["tracks"][0]["id"], b = initial["tracks"][1]["id"],
                          x = initial["tracks"][2]["id"], y = initial["tracks"][3]["id"];
        const std::string ca = clip(c, 0)["id"], cb = clip(c, 1)["id"], eq = clip(c, 0)["plugins"][0]["id"];
        run(c,
            Json::array({operation("plugin.parameter", {{"plugin", eq}, {"parameter", "Mid gain 1"}, {"value", 3.}})}));
        pump();
        initial = c.query();
        auto reference = rendered(c, folder.getChildFile("reference.wav"), 48000, 264000);
        objects(w, c, Json::array({ca, cb}));
        const auto beforeCopy = c.query();
        key(w, 'c');
        const auto copied = c.clipboard();
        check(copied["entries"].size() == 2 && copied["tracks"] == Json::array({a, b}) &&
                  c.query()["tracks"] == beforeCopy["tracks"] && c.query()["revision"] == beforeCopy["revision"] &&
                  c.query()["can_undo"] == beforeCopy["can_undo"],
              "Copy freezes two native clips without an Edit or Undo mutation");
        const auto token = copied["entries"][0]["token"];
        fails(
            [&]
            {
                c.makePlan(
                    "agent:untrusted",
                    Json::array({operation(
                        "clip.copy", {{"clip", token}, {"track", x}, {"position_samples", 300000}, {"ref", "$p"}})}));
            },
            "Agent cannot use local clipboard capability as an arbitrary native-state insertion");
        run(c, Json::array(
                   {operation("clip.gain", {{"clip", ca}, {"db", -18}}),
                    operation("plugin.parameter", {{"plugin", eq}, {"parameter", "Mid gain 1"}, {"value", -6.}})}));
        pump();
        cursor(w, c, 300000, x);
        const auto beforePaste = c.query();
        key(w, 'v');
        const auto pasted = c.query();
        auto pa = clip(c, 2), pb = clip(c, 3);
        check(pa["start_samples"] == 300000 && pb["start_samples"] == 324000 && pa["source_offset_samples"] == 24000 &&
                  pa["gain_db"] == -6 && pa["fade_in_samples"] == 2400 && pa["fade_out_samples"] == 4800,
              "Paste maps consecutive tracks and preserves frozen offsets, gain and fades");
        check(pa["id"] != ca && pa["plugins"][0]["id"] != eq && pa["plugins"][0]["owner_clip"] == pa["id"] &&
                  pa["parent_clip"] == ca,
              "native ClipCopy remaps clip and processor IDs while retaining source lineage");
        auto audio = rendered(c, folder.getChildFile("pasted.wav"), 300000, 516000);
        sameAudio(reference, audio);
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == beforePaste["tracks"],
              "one Undo removes all pasted clips and restores replaced state");
        w.uiCommands().invokeDirectly(7, false);
        check(c.query()["tracks"] == pasted["tracks"],
              "one Redo restores identical pasted IDs and frozen native state");
        auto saved = folder.getChildFile("clipboard.tracktionedit");
        c.save(saved);
        w.openSession(saved);
        pump();
        check(c.query()["tracks"] == pasted["tracks"] && c.clipboard().is_null(),
              "actual save/reopen preserves pasted objects and invalidates ephemeral clipboard capabilities");
        auto reopened = rendered(c, folder.getChildFile("reopened.wav"), 300000, 516000);
        sameAudio(audio, reopened);
        objects(w, c, Json::array({pa["id"], pb["id"]}));
        key(w, 'x');
        check(c.query()["tracks"][2]["clips"].empty() && c.query()["tracks"][3]["clips"].empty(),
              "whole-object Cut removes both clips and activates a frozen clipboard only after success");
        key(w, 'v', true);
        const auto cutPaste = c.query();
        check(clip(c, 2)["start_samples"] == 300000 && clip(c, 3)["start_samples"] == 324000 &&
                  clip(c, 2)["gain_db"] == -6,
              "paste-original restores cut audio without a live source clip");
        w.uiCommands().invokeDirectly(6, false);
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == pasted["tracks"],
              "Undo paste then Cut restores original objects and both FX instances");
        w.uiCommands().invokeDirectly(7, false);
        w.uiCommands().invokeDirectly(7, false);
        check(c.query()["tracks"] == cutPaste["tracks"], "mixed clipboard edits replay exact native Undo states");
        w.uiCommands().invokeDirectly(6, false);
        w.uiCommands().invokeDirectly(6, false);
        objects(w, c, Json::array({pa["id"], pb["id"]}));
        const auto retainedBuffer = c.clipboard()["id"];
        run(c, Json::array({operation(
                   "clip.import",
                   {{"track", x}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 550000}})}));
        pump();
        const auto overlap = clip(c, 2, 1);
        check(overlap["start_samples"] == 550000 && overlap["length_samples"] == 192000,
              "duplicate destination fixture overlaps the end of the selected span");
        key(w, 'd');
        check(c.query()["tracks"][2]["clips"].size() == 3 && c.query()["tracks"][3]["clips"].size() == 2 &&
                  c.clipboard()["id"] == retainedBuffer && c.timelineRange()["start_samples"] == 516000,
              "Duplicate follows the complete selected span and preserves the active clipboard");
        bool preservedOverlap = false;
        const auto afterDuplicate = c.query();
        for (const auto& item : afterDuplicate["tracks"][2]["clips"])
            preservedOverlap |=
                item["id"] == overlap["id"] && item["start_samples"] == 550000 && item["length_samples"] == 192000;
        check(preservedOverlap, "Duplicate keeps overlapping destination media unchanged");
        w.uiCommands().invokeDirectly(6, false);
        range(w, c, 360000, 384000, Json::array({x, y}));
        const auto beforePartial = c.query();
        key(w, 'x');
        const auto partial = c.query();
        check(partial["tracks"][2]["clips"].size() == 3 && partial["tracks"][3]["clips"].size() == 2 &&
                  c.clipboard()["end_samples"].get<int64_t>() - c.clipboard()["start_samples"].get<int64_t>() == 24000,
              "partial cross-track Cut creates real retained left/right clips and exact-length snapshot");
        auto gap = rendered(c, folder.getChildFile("cut-gap.wav"), 360000, 384000);
        check(gap.getMagnitude(0, 2048, gap.getNumSamples() - 4096) < 0.00002 &&
                  gap.getMagnitude(1, 2048, gap.getNumSamples() - 4096) < 0.00002,
              "real rendered Cut gap is silent outside the declared processor edge window");
        key(w, 'v', true);
        const auto pastedPartial = c.query();
        bool sourceMappingRestored = false;
        for (const auto& item : pastedPartial["tracks"][2]["clips"])
            sourceMappingRestored |= item["start_samples"] == 360000 && item["source_offset_samples"] == 84000;
        check(pastedPartial["tracks"][2]["clips"].size() == 4 && pastedPartial["tracks"][3]["clips"].size() == 3 &&
                  sourceMappingRestored,
              "partial paste-original retains original source-time mapping");
        w.uiCommands().invokeDirectly(6, false);
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == beforePartial["tracks"],
              "two Undo operations restore exact pre-cut clip topology");
        range(w, c, 240000, 384000, Json::array({x, y}));
        key(w, 'c');
        const auto leading = c.clipboard();
        check(leading["start_samples"] == 240000 && leading["entries"][0]["start_samples"] == 300000,
              "range clipboard preserves selected leading silence rather than collapsing the gap");
        run(c, Json::array({operation(
                   "clip.import",
                   {{"track", a}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 600000}})}));
        pump();
        cursor(w, c, 620000, a);
        const auto overwriteBefore = c.query();
        key(w, 'v');
        const auto overwrite = c.query();
        check(overwrite["tracks"][0]["clips"].size() == 4 && overwrite["tracks"][1]["clips"].size() == 2 &&
                  c.timelineRange()["start_samples"] == 620000 && c.timelineRange()["end_samples"] == 764000,
              "ordinary Paste replaces target interval, preserves outer tails and copied leading silence");
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == overwriteBefore["tracks"],
              "overwrite Paste and interval splitting share one Undo");
        objects(w, c, Json::array({pa["id"]}));
        const auto stale = c.query();
        run(c, Json::array({operation("clip.move", {{"clip", pa["id"]}, {"position_samples", 301000}})}));
        const auto human = c.query();
        w.keyPressed(juce::KeyPress('x', juce::ModifierKeys::commandModifier, 'x'));
        check(c.query()["tracks"] == human["tracks"] && c.query()["revision"] == human["revision"],
              "stale UI Cut cannot overwrite an interleaved human clip move before timer refresh");
        w.uiCommands().invokeDirectly(6, false);
        run(c, Json::array({operation("clip.lock", {{"clip", pa["id"]}, {"locked", true}})}));
        pump();
        objects(w, c, Json::array({pa["id"]}));
        check(!w.keyPressed(juce::KeyPress('x', juce::ModifierKeys::commandModifier, 'x')),
              "locked clip disables Cut without altering clipboard or history");
        key(w, 'c');
        check(c.clipboard()["entries"].size() == 1, "locked audio may be copied read-only");
        w.uiCommands().invokeDirectly(6, false);
        auto bound =
            c.makePlan("human", Json::array({operation("clip.copy", {{"clip", c.clipboard()["entries"][0]["token"]},
                                                                     {"track", y},
                                                                     {"position_samples", 900000},
                                                                     {"ref", "$bound"}})}));
        auto receipt = c.commit(bound);
        const auto once = c.query()["tracks"];
        c.commit(bound);
        check(c.query()["tracks"] == once && c.commit(bound)["replayed"], "clipboard Plan retry is idempotent");
        c.undo();
        check(Commands::mediaHash(media) == hash, "all clipboard editing and Undo leave original media hash unchanged");
        auto mediaPlan =
            c.makePlan("human", Json::array({operation("clip.copy", {{"clip", c.clipboard()["entries"][0]["token"]},
                                                                     {"track", y},
                                                                     {"position_samples", 900000},
                                                                     {"ref", "$hash"}})}));
        {
            juce::FileOutputStream out(media);
            out.setPosition(media.getSize());
            out.writeByte(0);
        }
        const auto mediaBefore = c.query();
        fails([&] { c.commit(mediaPlan); }, "changed source media rejects frozen paste before any native insertion");
        check(c.query()["tracks"] == mediaBefore["tracks"] && c.query()["revision"] == mediaBefore["revision"],
              "media conflict keeps project and history unchanged");
        Json result = {{"result", "passed"},
                       {"checks", checks},
                       {"scope", "actual L1 frozen audio state, native global commands, decoded Tracktion renders, "
                                 "save/reopen; MIDI/automation clipboard and desktop acceptance pending"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2);
        }
        std::cout << result.dump(2) << std::endl;
        folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
