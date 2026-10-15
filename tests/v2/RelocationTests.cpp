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
    static void refresh(Workspace& w)
    {
        w.refresh();
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
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
}
template <class F> void wait(F f, const char* why)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump();
    check(f(), why);
}
template <class F> void rejects(F f, const char* why)
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
class Storage : public te::PropertyStorage
{
    juce::File root;

public:
    Storage(juce::File f) : PropertyStorage("forma-relocation-tests"), root(f)
    {
        root.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
Json run(Commands& c, const char* command, Json args)
{
    return c.commit(c.makePlan("human", Json::array({op(command, std::move(args))})));
}
Json all(Commands& c)
{
    return c.query()["tracks"];
}
Json arrangement(Commands& c)
{
    auto result = all(c);
    for (auto& track : result)
    {
        const auto query = c.automationQuery(track["id"]);
        for (const auto& lane : query["lanes"])
            if (!lane["points"].empty())
            {
                if (lane["parameter"] == "volume")
                    track.erase("gain_db");
                if (lane["parameter"] == "pan")
                    track.erase("pan");
            }
    }
    return result; // Explicit base values remain part of the assertion.
}
Json clip(Commands& c, const std::string& id)
{
    for (const auto& t : all(c))
        for (const auto& p : t["clips"])
            if (p["id"] == id)
                return p;
    throw std::runtime_error("missing clip");
}
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
EditWindow& area(Workspace& w)
{
    auto* e = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
    check(e != nullptr, "real native Edit timeline exists");
    return *e;
}
auto mouse(juce::Component& e, juce::Point<float> p, bool shift = false, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(
        juce::Desktop::getInstance().getMainMouseSource(), p,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | (shift ? juce::ModifierKeys::shiftModifier : 0)), 1,
        0, 0, 0, 0, &e, &e, now, p, now, 1, dragged);
}
juce::Point<float> body(Workspace& w, const std::string& id)
{
    auto& c = RecordingTestAccess::commands(w);
    int row = 0;
    for (const auto& t : all(c))
    {
        if (t.value("edit_hidden", false))
            continue;
        for (const auto& p : t["clips"])
            if (p["id"] == id)
            {
                const auto r = area(w).clipRect(p, row);
                return {float(r.getCentreX()), float(r.getBottom() - 12)};
            }
        ++row;
    }
    throw std::runtime_error("visible anchor missing");
}
void choose(Workspace& w, const std::string& id, bool shift = false)
{
    auto& e = area(w);
    const auto p = body(w, id);
    e.mouseDown(mouse(e, p, shift));
    e.mouseUp(mouse(e, p, shift));
}
struct Drag
{
    juce::Point<float> to;
    int64_t delta;
};
Drag begin(Workspace& w, const std::string& id, int pixels)
{
    auto& e = area(w);
    const auto p = body(w, id), q = p + juce::Point<float>(float(pixels), 0);
    const auto axis = e.coordinates();
    e.mouseDown(mouse(e, p));
    e.mouseDrag(mouse(e, q, false, true));
    return {q, std::llround(pixels * axis.span / axis.width)};
}
void finish(Workspace& w, const Drag& d)
{
    auto& e = area(w);
    e.mouseUp(mouse(e, d.to, false, true));
}
void configure(Workspace& w)
{
    w.setSize(1600, 1000);
    w.setVisible(true);
    auto& c = RecordingTestAccess::commands(w);
    c.updateUiState({{"workspace", "edit"}, {"edit_mode", "slip"}, {"span_samples", 720000}, {"row_height", 108}},
                    c.sessionToken());
    RecordingTestAccess::refresh(w);
    check(w.keyPressed(juce::KeyPress(juce::KeyPress::F8Key)), "registered F8 activates native Grabber");
}
juce::File source(juce::File folder)
{
    auto f = folder.getChildFile("Source.wav");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out = f.createOutputStream();
    auto wr = wav.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(wr), "real stereo PCM24 writer available");
    juce::AudioBuffer<float> p(2, 96000);
    for (int i = 0; i < p.getNumSamples(); ++i)
    {
        p.setSample(0, i, float(.09 * std::sin(i * .071)));
        p.setSample(1, i, float(.05 * std::sin(i * .113)));
    }
    check(wr->writeFromAudioSampleBuffer(p, 0, p.getNumSamples()), "actual non-silent PCM source written");
    return f;
}
juce::AudioBuffer<float> render(Commands& c, juce::File folder)
{
    wait([&] { return !c.playbackPreparation()["busy"].get<bool>(); }, "source preparation within original 5s budget");
    auto f = folder.getChildFile("Render-" + juce::String(++serial) + ".wav");
    auto receipt = c.render(f, 0, 480000);
    check(receipt["audio_verified"] && receipt["render_ms"].get<double>() < 10000,
          "actual native WAV completes within 10s");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> rd(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(rd && rd->sampleRate == 48000 && rd->bitsPerSample == 24 && rd->numChannels == 2 &&
              rd->lengthInSamples == 480000,
          "independent WAV reader verifies format and 480000 frames");
    juce::AudioBuffer<float> p(2, 480000);
    check(rd->read(&p, 0, p.getNumSamples(), 0, true, true), "actual rendered PCM decodes");
    check(p.getMagnitude(0, p.getNumSamples()) > .001f, "render contains actual non-silent signal");
    return p;
}
void moved(Commands& c, const Json& before, int64_t delta)
{
    int count = 0;
    for (const auto& t : before)
        for (const auto& p : t["clips"])
        {
            auto after = clip(c, p["id"]);
            check(after["start_samples"].get<int64_t>() == p["start_samples"].get<int64_t>() + delta,
                  "every selected native identity moves by one exact common sample delta");
            check(after["length_samples"] == p["length_samples"], "constant-tempo native duration preserved");
            if (p["kind"] == "midi")
                for (size_t i = 0; i < p["notes"].size(); ++i)
                {
                    const auto& a = after["notes"][i];
                    const auto& n = p["notes"][i];
                    check(a["id"] == n["id"] && a["pitch"] == n["pitch"] && a["velocity"] == n["velocity"] &&
                              a["position_samples"].get<int64_t>() == n["position_samples"].get<int64_t>() + delta,
                          "real MIDI event identity, expression and exact project timing preserved");
                }
            ++count;
        }
    check(count > 0, "moved state includes actual clips");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P3-Relocation-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto file = source(root);
        const auto hash = Commands::mediaHash(file);
        Workspace w(false, std::make_unique<Storage>(root.getChildFile("prefs")));
        auto& c = RecordingTestAccess::commands(w);
        RecordingTestAccess::refresh(w);
        const juce::KeyPress tailKey(
            '1', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        check(w.keyPressed(tailKey), "registered tail shortcut dispatches through the real command target");
        check(c.editingOptions()["relocation_tail_samples"] == 24000, "shortcut commits actual half-second setting");
        const auto settings = root.getChildFile("Tail-settings.tracktionedit");
        c.save(settings);
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("tail-cold")));
            cold.open(settings);
            check(cold.editingOptions()["relocation_tail_samples"] == 24000, "tail setting survives actual cold Open");
            cold.undo();
            check(cold.editingOptions()["relocation_tail_samples"] == 96000,
                  "cold original Undo restores default continuation");
            cold.redo();
            check(cold.editingOptions()["relocation_tail_samples"] == 24000,
                  "cold original Redo restores persisted setting");
        }
        c.undo();
        check(c.editingOptions()["relocation_tail_samples"] == 96000, "live Undo restores two-second setting");
        c.commit(c.makePlan(
            "human",
            Json::array(
                {op("track.create", {{"name", "Audio A"}, {"ref", "$a"}}),
                 op("clip.import",
                    {{"track", "$a"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 48000}}),
                 op("track.create", {{"name", "Audio B"}, {"ref", "$b"}}),
                 op("clip.import",
                    {{"track", "$b"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 600000}}),
                 op("track.create", {{"name", "MIDI A"}, {"type", "midi"}, {"ref", "$ma"}}),
                 op("midi.clip.create", {{"track", "$ma"},
                                         {"ref", "$m"},
                                         {"name", "MIDI A"},
                                         {"position_samples", 96000},
                                         {"length_samples", 96000}}),
                 op("midi.note.add", {{"clip", "$m"},
                                      {"pitch", 67},
                                      {"velocity", 83},
                                      {"position_samples", 108000},
                                      {"length_samples", 24000}}),
                 op("track.create", {{"name", "MIDI B"}, {"type", "midi"}, {"ref", "$mb"}}),
                 op("midi.clip.create", {{"track", "$mb"},
                                         {"ref", "$placeholder"},
                                         {"name", "Destination"},
                                         {"position_samples", 600000},
                                         {"length_samples", 24000}})})));
        auto q = all(c);
        const std::string a = q[0]["clips"][0]["id"], b = q[1]["clips"][0]["id"], m = q[2]["clips"][0]["id"],
                          destination = q[3]["clips"][0]["id"];
        const std::string at = q[0]["id"], bt = q[1]["id"], mt = q[2]["id"], dt = q[3]["id"];
        run(c, "midi.cc.replace",
            {{"clip", m},
             {"controller", 11},
             {"state_hash", clip(c, m)["controller_state_hash"]},
             {"points", Json::array({{{"id", "cc:" + m + ":new:" + juce::Uuid().toString().toStdString()},
                                      {"source_beat", 0.0},
                                      {"raw_value", 4096}},
                                     {{"id", "cc:" + m + ":new:" + juce::Uuid().toString().toStdString()},
                                      {"source_beat", 1.0},
                                      {"raw_value", 12288}}})}});
        auto where = [&](const std::string& id)
        {
            for (const auto& t : all(c))
                for (const auto& x : t["clips"])
                    if (x["id"] == id)
                        return t["id"].get<std::string>();
            throw std::runtime_error("missing owner");
        };
        auto accept = [&]
        {
            auto* button = dynamic_cast<juce::Button*>(find(w, "plan.accept"));
            check(button && button->isEnabled(), "actual relocation confirmation is enabled");
            const auto rev = c.query()["revision"];
            button->triggerClick();
            wait([&] { return c.query()["revision"] != rev; }, "actual asynchronous acceptance commits a new revision");
        };
        auto drop = [&](const std::string& from, const std::string& to, int pixels = 0)
        {
            auto& e = area(w);
            auto p = body(w, from), target = body(w, to);
            target.x = p.x + pixels;
            e.mouseDown(mouse(e, p));
            e.mouseDrag(mouse(e, target, false, true));
            check(where(from) == (from == a ? at : mt), "vertical preview does not reparent the real object");
            e.mouseUp(mouse(e, target, false, true));
        };
        configure(w);
        choose(w, a);
        const auto before = all(c);
        const auto reference = render(c, root);
        drop(a, b);
        check(all(c) == before, "unaccepted audio drop leaves complete project state intact");
        accept();
        check(where(a) == bt && clip(c, a)["id"] == a && clip(c, a)["start_samples"] == 48000,
              "pure vertical GUI move preserves original audio identity and exact position");
        check(clip(c, b) == before[1]["clips"][0], "existing destination clip is never overwritten");
        check(w.queryView()["object_selection"][0]["track"] == bt,
              "selection ownership follows real native ID into destination");
        const auto pcm = render(c, root);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 256; i < reference.getNumSamples() - 256; ++i)
                maxError = std::max(maxError, std::abs(double(reference.getSample(ch, i)) - pcm.getSample(ch, i)));
        check(maxError <= 2e-6, "actual vertical native graph preserves summed stereo PCM within original budget");
        const auto after = all(c);
        const auto saved = root.getChildFile("Audio-Relocation.tracktionedit");
        c.save(saved);
        const auto token = c.sessionToken();
        w.openSession(saved);
        check(c.sessionToken() != token && all(c) == after && w.queryView()["object_selection"][0]["track"] == bt,
              "real saved project reopens with native ownership and selection");
        c.undo();
        RecordingTestAccess::refresh(w);
        check(all(c) == before && w.queryView()["object_selection"][0]["track"] == at,
              "original pre-save relocation Undo restores both owners and stable selection");
        c.redo();
        RecordingTestAccess::refresh(w);
        check(all(c) == after && where(a) == bt, "original relocation Redo restores actual destination");
        const juce::KeyPress up(juce::KeyPress::upKey,
                                juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier |
                                    juce::ModifierKeys::altModifier,
                                0);
        check(w.keyPressed(up), "registered cross-track shortcut enters actual confirmation path");
        check(where(a) == bt, "keyboard request also waits for acceptance");
        accept();
        check(where(a) == at, "keyboard relocation moves same audio identity back to previous track");
        c.undo();
        c.undo();
        RecordingTestAccess::refresh(w);
        check(all(c) == before, "two original relocation Undos restore initial arrangement");
        choose(w, a);
        const auto locked = run(c, "clip.lock", {{"clip", a}, {"locked", true}});
        rejects(
            [&]
            {
                c.makePlan(
                    "human",
                    Json::array({op(
                        "clips.relocate",
                        {{"moves",
                          Json::array(
                              {{{"clip", a}, {"from_track", at}, {"to_track", bt}, {"position_samples", 48000}}})}})}));
            },
            "source clip lock rejects the whole relocation Plan");
        c.undo(locked["plan_id"]);
        RecordingTestAccess::refresh(w);
        drop(a, destination);
        auto* invalid = dynamic_cast<juce::Label*>(find(w, "workspace.status"));
        check(where(a) == at && invalid && invalid->getText().contains(text("类型不匹配")),
              "incompatible MIDI destination refuses real audio drop with visible explanation");
        choose(w, m);
        const auto midiBefore = clip(c, m);
        drop(m, destination);
        accept();
        const auto midiAfter = clip(c, m);
        check(where(m) == dt && midiAfter["id"] == m && midiAfter["notes"] == midiBefore["notes"] &&
                  midiAfter["controller_events"] == midiBefore["controller_events"],
              "pure vertical MIDI move retains actual sequence identities and event timing");
        const auto midiFile = root.getChildFile("MIDI-Relocation.tracktionedit");
        c.save(midiFile);
        const auto midiToken = c.sessionToken();
        w.openSession(midiFile);
        check(c.sessionToken() != midiToken && where(m) == dt && clip(c, m) == midiAfter,
              "MIDI relocation survives real native Open with unchanged sequence");
        c.undo();
        RecordingTestAccess::refresh(w);
        check(where(m) == mt && clip(c, m) == midiBefore,
              "cold original MIDI relocation Undo restores exact event source");
        c.redo();
        check(where(m) == dt, "cold original MIDI relocation Redo restores target");
        c.undo();
        RecordingTestAccess::refresh(w);
        Json moves = Json::array({{{"clip", a}, {"from_track", at}, {"to_track", bt}, {"position_samples", 72000}},
                                  {{"clip", m}, {"from_track", mt}, {"to_track", dt}, {"position_samples", 120000}}});
        run(c, "group.create",
            {{"id", "relocation-group"},
             {"name", "Mixed relocation group"},
             {"members", Json::array({at, mt})},
             {"enabled", true},
             {"edit", true},
             {"mute", false},
             {"solo", false}});
        const auto grouped = all(c);
        rejects([&] { c.makePlan("human", Json::array({op("clips.relocate", {{"moves", Json::array({moves[0]})}})})); },
                "incomplete overlapping mixed Edit group refuses the entire relocation");
        check(all(c) == grouped, "rejected incomplete group leaves every actual owner unchanged");
        auto plan = c.makePlan("human", Json::array({op("clips.relocate", {{"moves", moves}})}));
        Scope denied;
        denied.targets = {at, mt};
        rejects([&] { c.review(plan, denied); }, "source-only permissions do not grant destination processing owners");
        Scope granted;
        granted.targets = {at, bt, mt, dt};
        c.review(plan, granted);
        const auto receipt = c.commit(plan);
        check(where(a) == bt && where(m) == dt, "one batch relocates actual audio and MIDI owners");
        check(c.commit(plan)["replayed"], "retry returns actual original receipt without duplicate clips");
        c.undo(receipt["plan_id"]);
        check(c.commit(plan)["state"] == "undone" && where(a) == at && where(m) == mt,
              "retry after Undo never moves restored objects again");
        auto stale = c.makePlan("human", Json::array({op("clips.relocate", {{"moves", moves}})}));
        run(c, "track.rename", {{"track", bt}, {"name", "Human destination edit"}});
        const auto human = all(c);
        rejects([&] { c.commit(stale); }, "human destination change rejects stale relocation Plan");
        check(all(c) == human, "conflict preserves human edit and all original clip owners");
        c.undo();
        run(c, "automation.point.add",
            {{"track", at},
             {"parameter", "volume"},
             {"ref", "$source-volume"},
             {"position_samples", 0},
             {"value", -6.0},
             {"curve", 0.0}});
        run(c, "automation.point.add",
            {{"track", bt},
             {"parameter", "volume"},
             {"ref", "$target-volume"},
             {"position_samples", 0},
             {"value", -3.0},
             {"curve", 0.0}});
        const auto automated = arrangement(c);
        const auto followed = c.makePlan("human", Json::array({op("clips.relocate", {{"moves", moves}})}));
        check(c.preview(followed)["relocation_changes"][0]["automation"]["transfers"].size() == 1 &&
                  arrangement(c) == automated,
              "enabled automation follow previews the actual matched volume transfer without changing Edit");
        c.commit(followed);
        check(where(a) == bt && where(m) == dt, "enabled automation follows the actual mixed relocation");
        c.undo();
        check(arrangement(c) == automated, "one original followed Undo restores both owners and curve base values");
        run(c, "session.automation_follows_edit.set", {{"enabled", false}});
        auto points = [&](const std::string& track)
        {
            Json result = Json::array();
            const auto query = c.automationQuery(track);
            for (const auto& lane : query["lanes"])
                result.push_back(lane["points"]);
            return result;
        };
        const auto sourcePoints = points(at), destinationPoints = points(bt);
        const auto noFollow = c.makePlan("human", Json::array({op("clips.relocate", {{"moves", moves}})}));
        check(c.preview(noFollow)["relocation_changes"][0]["automation_policy"] ==
                  "track curves explicitly stay in place",
              "preview declares exact user-selected automation policy");
        c.commit(noFollow);
        check(where(a) == bt && where(m) == dt && points(at) == sourcePoints && points(bt) == destinationPoints,
              "explicit track automation policy preserves every real point identity, time and value");
        c.undo();
        check(where(a) == at && where(m) == mt && points(at) == sourcePoints && points(bt) == destinationPoints,
              "one policy-bound Undo restores owners without rewriting unrelated curves");
        const auto mediaPlan = c.makePlan("human", Json::array({op("clips.relocate", {{"moves", moves}})}));
        juce::MemoryBlock original;
        check(file.loadFileAsData(original) && original.getSize() > 100,
              "own PCM source bytes are available for actual media-fault injection");
        const auto time = file.getLastModificationTime();
        auto altered = original;
        static_cast<unsigned char*>(altered.getData())[altered.getSize() - 17] ^= 1;
        check(file.replaceWithData(altered.getData(), altered.getSize()) && file.setLastModificationTime(time),
              "actual same-size PCM byte mutation retains original modification time");
        auto restore = [&]
        {
            if (!file.replaceWithData(original.getData(), original.getSize()) || !file.setLastModificationTime(time))
                throw std::runtime_error("could not restore own media-fault fixture");
        };
        try
        {
            rejects([&] { c.preview(mediaPlan); },
                    "media bytes changed at same size/time invalidate actual sealed relocation");
        }
        catch (...)
        {
            restore();
            throw;
        }
        restore();
        check(Commands::mediaHash(file) == hash, "relocation and recovery leave original media bytes unchanged");
        Json result{
            {"result", "passed"},
            {"checks", checks},
            {"max_pcm_error", maxError},
            {"session", saved.getFullPathName().toStdString()},
            {"midi_session", midiFile.getFullPathName().toStdString()},
            {"scope",
             "native JUCE pointer/confirmation/key and actual L1/Undo/PCM; no physical GUI/hardware/listening claim"}};
        if (argc > 1)
            std::ofstream(argv[1]) << result.dump(2) << '\n';
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
