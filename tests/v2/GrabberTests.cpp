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
    Storage(juce::File f) : PropertyStorage("forma-grabber-tests"), root(f)
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
    auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P3-Grabber-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        auto file = source(root);
        const auto hash = Commands::mediaHash(file);
        std::string sessionPath;
        {
            Workspace w(false, std::make_unique<Storage>(root.getChildFile("mixed-prefs")));
            auto& c = RecordingTestAccess::commands(w);
            c.commit(c.makePlan(
                "human",
                Json::array(
                    {op("track.create", {{"name", "Audio A"}, {"ref", "$a"}}),
                     op("clip.import",
                        {{"track", "$a"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 48000}}),
                     op("track.create", {{"name", "Audio B"}, {"ref", "$b"}}),
                     op("clip.import",
                        {{"track", "$b"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 24000}}),
                     op("track.create", {{"name", "MIDI"}, {"type", "midi"}, {"ref", "$m"}}),
                     op("midi.clip.create", {{"track", "$m"},
                                             {"ref", "$mc"},
                                             {"name", "Notes"},
                                             {"position_samples", 96000},
                                             {"length_samples", 96000}}),
                     op("midi.note.add", {{"clip", "$mc"},
                                          {"pitch", 64},
                                          {"velocity", 87},
                                          {"position_samples", 108000},
                                          {"length_samples", 24000}})})));
            auto q = all(c);
            const std::string a = q[0]["clips"][0]["id"], b = q[1]["clips"][0]["id"], m = q[2]["clips"][0]["id"];
            const std::string at = q[0]["id"], bt = q[1]["id"];
            configure(w);
            choose(w, a);
            choose(w, b, true);
            choose(w, m, true);
            check(w.queryView()["object_selection"].size() == 3,
                  "Shift body clicks select actual audio and MIDI clips");
            const auto before = all(c);
            const auto baseline = render(c, root);
            const auto rev = c.query()["revision"].get<uint64_t>();
            auto d = begin(w, a, 80);
            check(w.queryView()["object_selection"].size() == 3,
                  "mousedown on selected audio retains complete mixed selection");
            check(all(c) == before && c.query()["revision"] == rev,
                  "all drag ghosts are local drafts without an Edit transaction");
            finish(w, d);
            check(c.query()["revision"] == rev + 1, "one release creates exactly one native transaction");
            moved(c, before, d.delta);
            const auto after = all(c);
            auto pcm = render(c, root);
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 256; n < baseline.getNumSamples() - int(d.delta) - 256; ++n)
                    maxError = std::max(
                        maxError, std::abs(double(baseline.getSample(ch, n)) - pcm.getSample(ch, n + int(d.delta))));
            check(maxError <= 2e-6, "actual summed stereo PCM retains phase within original 2e-6 budget");
            check(w.keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)),
                  "actual command-key router dispatches Undo");
            wait([&] { return all(c) == before; }, "one native Undo restores every audio/MIDI member");
            check(w.keyPressed(
                      juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)),
                  "actual command-key router dispatches Redo");
            wait([&] { return all(c) == after; }, "one Redo restores the entire selected move");
            auto saved = root.getChildFile("Mixed-Grabber.tracktionedit");
            c.save(saved);
            sessionPath = saved.getFullPathName().toStdString();
            const auto view = w.queryView();
            const auto token = c.sessionToken();
            w.openSession(saved);
            if (all(c) != after)
                std::cerr << "GRABBER_REOPEN_TRACKS " << Json::diff(after, all(c)).dump(2) << std::endl;
            if (w.queryView() != view)
                std::cerr << "GRABBER_REOPEN_VIEW " << Json::diff(view, w.queryView()).dump(2) << std::endl;
            check(c.sessionToken() != token && all(c) == after && w.queryView() == view,
                  "cold native Open restores moved state and stable mixed selection");
            c.undo();
            check(all(c) == before, "original pre-save move Undo survives reopen");
            c.redo();
            check(all(c) == after, "original pre-save Redo survives reopen");
            c.undo();
            RecordingTestAccess::refresh(w);
            d = begin(w, m, 40);
            check(w.queryView()["object_selection"].size() == 3, "selected MIDI can anchor the same mixed draft");
            check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)), "Escape cancels actual active clip gesture");
            finish(w, d);
            check(all(c) == before, "mouse release after Escape cannot commit cancelled move");
            const auto midiAnchorView = w.queryView();
            const auto midiAnchorFile = root.getChildFile("MIDI-Anchor.tracktionedit");
            c.save(midiAnchorFile);
            const auto midiToken = c.sessionToken();
            w.openSession(midiAnchorFile);
            check(c.sessionToken() != midiToken && all(c) == before && w.queryView() == midiAnchorView,
                  "MIDI primary anchor and editor target also survive a real native reopen");
            auto* caption = dynamic_cast<ClipHeader*>(find(w, "clip.select:" + text(a)));
            check(caption != nullptr, "actual native clip-name control exists");
            auto& timeline = area(w);
            const auto from = caption->getBounds().getCentre().toFloat();
            const auto to = from + juce::Point<float>(40, 0);
            const auto captionDelta = std::llround(40 * timeline.coordinates().span / timeline.coordinates().width);
            caption->mouseDown(mouse(*caption, caption->getLocalPoint(&timeline, from)));
            caption->mouseDrag(mouse(*caption, caption->getLocalPoint(&timeline, to), false, true));
            check(all(c) == before && w.queryView()["object_selection"].size() == 3,
                  "real caption forwards a mixed selection draft without writing Edit");
            caption->mouseUp(mouse(*caption, caption->getLocalPoint(&timeline, to), false, true));
            moved(c, before, captionDelta);
            c.undo();
            check(all(c) == before, "one caption-drag Undo restores all selected native clips");
            RecordingTestAccess::refresh(w);
            choose(w, b, true);
            check(w.queryView()["object_selection"].size() == 2,
                  "Shift-click removes one selected object without moving it");
            choose(w, b, true);
            check(w.queryView()["object_selection"].size() == 3, "Shift-click can add object back");
            d = begin(w, a, 50);
            run(c, "track.rename", {{"track", at}, {"name", "Human renamed"}});
            const auto human = all(c);
            finish(w, d);
            check(all(c) == human, "stale released draft cannot overwrite interleaved human edit");
            c.undo();
            RecordingTestAccess::refresh(w);
            run(c, "clip.lock", {{"clip", b}, {"locked", true}});
            RecordingTestAccess::refresh(w);
            const auto locked = all(c);
            d = begin(w, a, 50);
            finish(w, d);
            check(all(c) == locked, "locked selected peer rejects the whole pointer gesture");
            c.undo();
            RecordingTestAccess::refresh(w);
            Scope limited;
            limited.commands = {"clip.move", "midi.clip.move"};
            limited.targets = {at};
            rejects(
                [&]
                {
                    c.review(
                        c.makePlan("human", Json::array({op("clip.move", {{"clip", a}, {"position_samples", 72000}}),
                                                         op("clip.move", {{"clip", b}, {"position_samples", 48000}})})),
                        limited);
                },
                "same L1 refuses multi-target move outside actor scope");
            run(c, "group.create",
                {{"id", "edit-pair"},
                 {"name", "Pair"},
                 {"enabled", true},
                 {"mute", false},
                 {"solo", false},
                 {"edit", true},
                 {"members", Json::array({at, bt})}});
            run(c, "track.create", {{"name", "Hidden folder"}, {"type", "folder"}, {"ref", "$folder"}});
            const auto folder = all(c).back()["id"];
            run(c, "track.parent", {{"track", bt}, {"parent", folder}});
            run(c, "track.collapsed", {{"track", folder}, {"enabled", true}});
            RecordingTestAccess::refresh(w);
            choose(w, a);
            choose(w, m, true);
            const auto grouped = all(c);
            d = begin(w, a, -500);
            finish(w, d);
            if (!(clip(c, b)["start_samples"] == 0 && clip(c, a)["start_samples"] == 24000 &&
                  clip(c, m)["start_samples"] == 72000))
            {
                auto* status = dynamic_cast<juce::Label*>(find(w, "workspace.status"));
                std::cerr << "HIDDEN_CLAMP_FAILURE " << clip(c, a)["start_samples"] << ","
                          << clip(c, b)["start_samples"] << "," << clip(c, m)["start_samples"]
                          << " selection=" << w.queryView()["object_selection"].dump()
                          << " status=" << (status ? status->getText().toStdString() : "missing") << std::endl;
            }
            check(clip(c, b)["start_samples"] == 0 && clip(c, a)["start_samples"] == 24000 &&
                      clip(c, m)["start_samples"] == 72000,
                  "hidden earlier Edit-group member constrains the common delta to actual session start");
            c.undo();
            check(all(c) == grouped, "one Undo restores visible and hidden grouped members");
        }
        {
            Workspace w(false, std::make_unique<Storage>(root.getChildFile("bulk-prefs")));
            auto& c = RecordingTestAccess::commands(w);
            run(c, "track.create", {{"name", "Bulk"}, {"ref", "$t"}});
            const auto t = all(c)[0]["id"];
            for (int i = 0; i < 12; ++i)
                run(c, "clip.import",
                    {{"track", t},
                     {"path", file.getFullPathName().toStdString()},
                     {"position_samples", 24000 + i * 36000}});
            auto original = all(c)[0]["clips"];
            for (const auto& p : original)
                run(c, "clip.trim",
                    {{"clip", p["id"]},
                     {"start_samples", p["start_samples"]},
                     {"end_samples", p["start_samples"].get<int64_t>() + 12000}});
            configure(w);
            original = all(c)[0]["clips"];
            for (size_t i = 0; i < original.size(); ++i)
                choose(w, original[i]["id"], i != 0);
            check(w.queryView()["object_selection"].size() == 12,
                  "twelve non-overlapping actual clips form one selection");
            const auto before = all(c);
            const auto d = begin(w, original[0]["id"], 30);
            finish(w, d);
            auto* accept = dynamic_cast<juce::Button*>(find(w, "plan.accept"));
            check(accept && accept->isVisible() && accept->isEnabled() && all(c) == before,
                  "large selection opens real unchanged-state confirmation preview");
            const auto rev = c.query()["revision"].get<uint64_t>();
            accept->triggerClick();
            wait([&] { return c.query()["revision"] != rev; }, "actual asynchronous Accept produces a commit receipt");
            moved(c, before, d.delta);
            c.undo();
            check(all(c) == before, "large accepted move has one Undo for all twelve members");
            RecordingTestAccess::refresh(w);
            const auto cancelled = begin(w, original[0]["id"], 30);
            finish(w, cancelled);
            auto* reject = dynamic_cast<juce::Button*>(find(w, "plan.reject"));
            check(reject && reject->isEnabled() && all(c) == before, "large drag can be rejected before changing Edit");
            reject->triggerClick();
            wait([&] { return !reject->isEnabled(); }, "actual asynchronous Reject clears the pending Plan");
            check(all(c) == before, "reject keeps every original clip unchanged");
            auto* map = w.uiCommands().getKeyMappings();
            const juce::KeyPress custom('g', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
            map->clearAllKeyPresses(editCommand::grabber);
            map->removeKeyPress(custom);
            map->addKeyPress(editCommand::grabber, custom);
            w.uiCommands().invokeDirectly(editCommand::selector, false);
            check(w.keyPressed(custom) && w.queryView()["edit_tool"] == "grabber",
                  "custom Grabber shortcut uses native command router");
            check(w.keyPressed(juce::KeyPress('.')), "existing Nudge shortcut remains usable for whole selected set");
            moved(c, before, 480);
            c.undo();
            check(all(c) == before, "Nudge shortcut shares one reversible native multi-target transaction");
            wait([&] { return !c.uiState()["keymap_xml"].get<std::string>().empty(); },
                 "custom command key change receives actual persistent UI receipt");
            const auto keyed = root.getChildFile("Custom-Grabber.tracktionedit");
            c.save(keyed);
            const auto oldSession = c.sessionToken();
            w.openSession(keyed);
            check(c.sessionToken() != oldSession, "custom shortcut fixture actually reopens a new session");
            w.uiCommands().invokeDirectly(editCommand::selector, false);
            check(w.keyPressed(custom) && w.queryView()["edit_tool"] == "grabber",
                  "custom Grabber key persists across real save and cold command-map restoration");
            check(Commands::mediaHash(file) == hash, "all gestures and history preserve original media bytes");
        }
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"max_pcm_error", maxError},
                    {"session", sessionPath},
                    {"scope", "real JUCE pointer/key/Accept callbacks and native L1/Undo/PCM; MIDI events only, no "
                              "physical key/mouse or MIDI hardware/listening claim"}};
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
