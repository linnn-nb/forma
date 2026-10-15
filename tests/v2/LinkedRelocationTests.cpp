// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class RecordingTestAccess
{
public:
    static Commands& commands(Workspace& w)
    {
        return w.commands;
    }
    static void select(Workspace& w, const std::string& clip)
    {
        w.refresh();
        w.selectAudioClip(clip);
    }
    static Json pending(Workspace& w)
    {
        return w.pending;
    }
    static te::WaveAudioClip* clip(Commands& c, const std::string& clip)
    {
        return c.audioClip(clip);
    }
    static std::string pairPlaylist(Commands& c, const std::string& track, const std::string& pair)
    {
        return c.track(track)
            ->state.getChildWithName("NDAW_CROSSFADES")
            .getChildWithProperty("id", juce::String(pair))["playlist"]
            .toString()
            .toStdString();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
double maxError = 0;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void rejects(F f, const char* why)
{
    bool rejected = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    check(rejected, why);
}
template <class F> void wait(F f, const char* why)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    check(f(), why);
}
Json op(const char* cmd, Json args)
{
    return {{"command", cmd}, {"args", args}};
}
Json run(Commands& c, const char* cmd, Json args)
{
    auto receipt = c.commit(c.makePlan("human", Json::array({op(cmd, args)})));
    check(receipt["state"] == "committed", "actual native transaction returns a committed receipt");
    return receipt;
}
class Storage : public te::PropertyStorage
{
    juce::File folder;

public:
    explicit Storage(juce::File f) : PropertyStorage("Forma linked relocation tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }
};
Json list(Commands& c, const std::string& track, const std::string& id)
{
    const auto facts = c.queryPlaylists(track);
    for (const auto& row : facts["items"])
        if (row["id"] == id)
            return row;
    throw std::runtime_error("actual Playlist missing");
}
Json state(Commands& c)
{
    Json result = Json::array();
    const auto facts = c.query();
    for (const auto& track : facts["tracks"])
    {
        auto playlists = c.queryPlaylists(track["id"]);
        playlists.erase("revision");
        playlists.erase("session_token");
        result.push_back({{"track", track["id"]}, {"playlists", playlists}, {"pairs", c.crossfades(track["id"])}});
    }
    return result;
}
juce::File source(juce::File folder, bool negative)
{
    auto file = folder.getChildFile(negative ? "Negative.wav" : "Positive.wav");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "actual PCM24 fixture writer opens");
    juce::AudioBuffer<float> pcm(2, 144000);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 144000; ++i)
            pcm.setSample(ch, i,
                          (negative ? -.125f : .125f) *
                              float(std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * i / 48000.)));
    check(writer->writeFromAudioSampleBuffer(pcm, 0, 144000), "real opposite-polarity source audio writes");
    return file;
}
juce::AudioBuffer<float> render(Commands& c, juce::File folder, int first = 0)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (c.playbackPreparation()["busy"].get<bool>() && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    check(!c.playbackPreparation()["busy"].get<bool>(), "actual source ready within fixed five-second budget");
    auto file = folder.getChildFile("Render-" + juce::String(++serial) + ".wav");
    const auto receipt = c.render(file, first, first + 144000);
    check(receipt["audio_verified"] && receipt["render_ms"].get<double>() < 10000,
          "actual native WAV render verified within fixed ten-second budget");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(reader && reader->sampleRate == 48000 && reader->bitsPerSample == 24 && reader->numChannels == 2 &&
              reader->lengthInSamples == 144000,
          "independent WAV decoder verifies PCM24 stereo sample rate and exact frames");
    juce::AudioBuffer<float> pcm(2, 144000);
    check(reader->read(&pcm, 0, 144000, 0, true, true), "actual exported PCM decodes");
    return pcm;
}
void compare(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int shift, const char* why)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 144000; ++i)
        {
            const double expected = i >= shift ? a.getSample(ch, i - shift) : 0.;
            error = std::max(error, std::abs(expected - b.getSample(ch, i)));
        }
    maxError = std::max(maxError, error);
    check(error <= 2e-6, why);
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
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    Json cases = Json::array();
    try
    {
        auto root =
            juce::File::getCurrentWorkingDirectory().getChildFile("P3-LinkedRelocation-" + juce::Uuid().toString());
        root.createDirectory();
        for (int mode = 0; mode < 3; ++mode)
        {
            auto folder = root.getChildFile(juce::String(mode));
            folder.createDirectory();
            auto positive = source(folder, false), negative = source(folder, true);
            const auto hashP = Commands::mediaHash(positive), hashN = Commands::mediaHash(negative);
            Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
            w.setSize(1280, 900);
            auto& c = RecordingTestAccess::commands(w);
            for (const char* name : {"Mic A", "Mic B", "Destination C", "Destination D"})
                run(c, "track.create", {{"name", name}, {"ref", "$t"}});
            const auto tracks = c.query()["tracks"];
            const std::string a = tracks[0]["id"], b = tracks[1]["id"], cc = tracks[2]["id"], d = tracks[3]["id"];
            std::map<std::string, std::string> takes, main, takeLineage;
            for (const auto& t : {a, b})
            {
                run(c, "track.gain", {{"track", t}, {"db", 0}});
                run(c, "clip.import",
                    {{"track", t},
                     {"path", (t == a ? positive : negative).getFullPathName().toStdString()},
                     {"position_samples", 0},
                     {"ref", "$source"}});
                main[t] = c.queryPlaylists(t)["active"];
                const auto clip = list(c, t, main[t])["clips"][0]["id"];
                run(c, "clip.fade",
                    {{"clip", clip},
                     {"in_samples", 40000},
                     {"out_samples", 30000},
                     {"in_curve", "linear"},
                     {"out_curve", "s_curve"}});
                takes[t] = run(c, "playlist.take.register",
                               {{"track", t},
                                {"source", main[t]},
                                {"clip", clip},
                                {"name", "Original source Take"}})["objects"][0]["id"];
                takeLineage[t] = list(c, t, takes[t])["take_id"];
            }
            for (const auto& t : {cc, d})
                run(c, "track.gain", {{"track", t}, {"db", 0}});
            std::string parked;
            if (mode == 1)
            {
                run(c, "clip.import",
                    {{"track", cc},
                     {"path", positive.getFullPathName().toStdString()},
                     {"position_samples", 600000},
                     {"ref", "$parked"}});
                parked = list(c, cc, c.queryPlaylists(cc)["active"])["clips"][0]["id"];
            }
            Json segments = Json::array();
            for (const auto& t : {a, b})
                for (const auto range : {std::pair<int, int>{12000, 60000}, {60000, 120000}})
                    segments.push_back({{"track", t},
                                        {"source", takes[t]},
                                        {"clip", list(c, t, takes[t])["clips"][0]["id"]},
                                        {"start_samples", range.first},
                                        {"end_samples", range.second}});
            const auto created = run(c, "playlist.comp.linked.create",
                                     {{"track", a}, {"name", "Phase linked Comp"}, {"segments", segments}});
            std::map<std::string, std::string> ids;
            for (const auto& object : created["objects"])
                if (object["kind"] == "playlist")
                    ids[object["track"]] = object["id"];
            run(c, "playlist.select", {{"track", a}, {"playlist", ids[a]}});
            const auto cut = c.crossfadeTarget(a, 60000);
            run(c, "playlist.crossfade",
                {{"track", a},
                 {"left", cut["left"]},
                 {"right", cut["right"]},
                 {"start_samples", 58800},
                 {"end_samples", 61200},
                 {"curve", "equal_power"}});
            check(c.crossfades(a).size() == 1 && c.crossfades(b).size() == 1,
                  "actual linked crossfade has both microphone records");
            run(c, "track.mute", {{"track", b}, {"enabled", true}});
            const auto reference = render(c, folder);
            check(reference.getMagnitude(0, 144000) > .01f, "positive Comp reference contains real audible signal");
            c.undo();
            juce::AudioBuffer<float> silence(2, 144000);
            silence.clear();
            compare(silence, render(c, folder), 0,
                    "actual original linked microphones phase null across the entire WAV");
            const auto before = state(c), originalA = list(c, a, takes[a]), originalB = list(c, b, takes[b]);
            const std::map<std::string, std::string> targets =
                mode == 0   ? std::map<std::string, std::string>{{a, cc}, {b, d}}
                : mode == 1 ? std::map<std::string, std::string>{{a, b}, {b, cc}}
                            : std::map<std::string, std::string>{{a, b}, {b, a}};
            const int shift = mode == 0 ? 1000 : 0;
            Json moves = Json::array();
            std::map<std::string, te::WaveAudioClip*> native;
            for (const auto& t : {a, b})
            {
                const auto sourceList = list(c, t, ids[t]);
                for (const auto& clip : sourceList["clips"])
                {
                    const std::string clipID = clip["id"];
                    native[clipID] = RecordingTestAccess::clip(c, clipID);
                    moves.push_back({{"clip", clipID},
                                     {"from_track", t},
                                     {"to_track", targets.at(t)},
                                     {"position_samples", clip["start_samples"].get<int64_t>() + shift}});
                }
            }
            auto plan = [&](const Json& m)
            { return c.makePlan("human", Json::array({op("clips.relocate", {{"moves", m}})})); };
            auto p = plan(moves);
            check(c.preview(p)["relocation_changes"][0]["linked_comp"][0]["members"].size() == 2 && state(c) == before,
                  "real preview exposes whole Playlist transfer without changing sources or targets");
            Scope partial;
            partial.targets = {a, b};
            if (mode != 2)
                rejects([&] { c.review(p, partial); }, "source-only grants cannot change destination playback");
            Scope bounded;
            bounded.begin = 0;
            bounded.end = 144000;
            rejects([&] { c.review(p, bounded); }, "clip-only time grant cannot retire an entire target Playlist");
            auto broken = moves;
            broken.erase(0);
            rejects([&] { plan(broken); }, "missing linked piece rejects the entire transfer");
            broken = moves;
            broken[0]["to_track"] = targets.at(b);
            rejects([&] { plan(broken); }, "one Playlist cannot be split between multiple destinations");
            run(c, "playlist.lock", {{"track", b}, {"playlist", ids[b]}, {"locked", true}});
            rejects([&] { plan(moves); }, "locked member rejects every destination change");
            c.undo();
            if (!parked.empty())
            {
                run(c, "clip.lock", {{"clip", parked}, {"locked", true}});
                rejects([&] { plan(moves); }, "locked unselected target material cannot be silently parked");
                c.undo();
            }
            p = plan(moves);
            run(c, "track.rename", {{"track", targets.at(a)}, {"name", "Human later edit"}});
            rejects([&] { c.commit(p); }, "later human edit rejects stale linked relocation");
            c.undo();
            check(state(c) == before, "all refusals and original Undos retain exact Playlist and pair state");
            p = plan(moves);
            if (mode == 1)
            {
                RecordingTestAccess::select(w, moves[0]["clip"]);
                check(
                    w.keyPressed(juce::KeyPress(juce::KeyPress::downKey,
                                                juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier |
                                                    juce::ModifierKeys::altModifier,
                                                0)),
                    "registered cross-track key resolves complete linked membership");
                p = RecordingTestAccess::pending(w);
                check(!p.is_null() && state(c) == before,
                      "native linked confirmation leaves engineering state untouched");
                auto* accept = dynamic_cast<juce::Button*>(find(w, "plan.accept"));
                check(accept && accept->isEnabled(), "actual native confirmation is enabled");
                const auto revision = c.querySummary()["revision"];
                accept->triggerClick();
                wait([&] { return c.querySummary()["revision"] != revision; },
                     "asynchronous native acceptance commits a real revision");
            }
            else
                check(c.commit(p)["state"] == "committed", "all native member transfers commit once");
            check(c.commit(p)["replayed"], "retry returns the real original receipt without duplicate Playlists");
            for (const auto& move : moves)
                check(RecordingTestAccess::clip(c, move["clip"]) == native.at(move["clip"]) &&
                          RecordingTestAccess::clip(c, move["clip"])->getTrack()->itemID.toString().toStdString() ==
                              move["to_track"].get<std::string>(),
                      "same runtime Clip and stable identity enter the actual destination");
            for (const auto& t : {a, b})
            {
                const auto current = list(c, targets.at(t), ids[t]);
                check(current["active"] && current["origin_track"] == t && current["linked_tracks"].size() == 2,
                      "Comp Playlist identity, origin owner and complete new membership preserved");
                for (const auto& clip : current["clips"])
                    check(clip["take_id"] == takeLineage[t] && clip["comp_origin_track"] == t &&
                              clip["take_origin_track"] == t,
                          "real Take lineage remains resolvable on its original track");
                check(c.crossfades(targets.at(t)).size() == 1 &&
                          RecordingTestAccess::pairPlaylist(c, targets.at(t), c.crossfades(targets.at(t))[0]["id"]) ==
                              ids[t],
                      "crossfade record follows the same Comp Playlist and target");
            }
            check(list(c, a, takes[a]) == originalA && list(c, b, takes[b]) == originalB,
                  "both original Take Playlists retained exactly");
            compare(silence, render(c, folder), 0, "relocated microphones retain actual full-WAV phase null");
            run(c, "track.mute", {{"track", targets.at(b)}, {"enabled", true}});
            compare(reference, render(c, folder), shift,
                    "one real relocated microphone preserves source PCM and inherited crossfade");
            c.undo();
            const auto after = state(c);
            const auto file = folder.getChildFile("Linked-Relocated.tracktionedit");
            c.save(file);
            {
                Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold")));
                cold.open(file);
                check(state(cold) == after,
                      "actual cold Open retains active membership, parked material and origin references");
                cold.undo(p["plan_id"]);
                check(state(cold) == before, "cold original Undo restores both sources and every destination Playlist");
                compare(silence, render(cold, folder), 0, "cold original Undo renders the original linked PCM");
                cold.redo();
                check(state(cold) == after, "cold original Redo restores exact linked transfer");
                compare(silence, render(cold, folder), 0, "cold original Redo renders relocated linked PCM");
            }
            run(c, "playlist.comp.restore", {{"track", targets.at(a)}, {"playlist", ids[a]}});
            for (const auto& t : {a, b})
                check(!list(c, targets.at(t), ids[t])["active"].get<bool>(),
                      "restore retires every relocated Comp member together");
            if (!parked.empty())
                check(render(c, folder, 600000).getMagnitude(0, 144000) > .01f,
                      "restore makes the preserved target's actual audio audible again");
            c.undo();
            check(state(c) == after, "one restore Undo reactivates the entire relocated set");
            c.undo(p["plan_id"]);
            check(state(c) == before, "live original relocation Undo restores all prior Playlists and pairs");
            c.redo();
            check(state(c) == after, "live Redo preserves all linked identities");
            check(Commands::mediaHash(positive) == hashP && Commands::mediaHash(negative) == hashN,
                  "all media bytes remain unchanged");
            cases.push_back({{"mode", mode == 0   ? "disjoint+1000samples"
                                      : mode == 1 ? "overlapping+occupied+native-key"
                                                  : "swap"},
                             {"session", file.getFullPathName().toStdString()},
                             {"plan_id", p["plan_id"]}});
        }
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"max_pcm_error", maxError},
                    {"pcm_tolerance", 2e-6},
                    {"cases", cases},
                    {"scope", "real native Edit, Workspace key/confirmation, PCM24 files and cold original UndoRedo; "
                              "no physical keyboard/microphone/listening claim"}};
        if (argc > 1)
            std::ofstream(argv[1]) << result.dump(2) << '\n';
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        Json result{{"result", "failed"}, {"checks", checks}, {"error", e.what()}, {"cases", cases}};
        if (argc > 1)
            std::ofstream(argv[1]) << result.dump(2) << '\n';
        std::cerr << result.dump(2) << std::endl;
        return 1;
    }
}
