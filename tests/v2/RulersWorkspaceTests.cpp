#include "Workspace.h"
#include "TimelineState.h"
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
    std::cout << "PASS " << why << '\n';
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
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File folder) : PropertyStorage("Forma ruler tests"), folder(folder) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (!p.isVisible())
        return nullptr;
    if (p.getComponentID() == id)
        return &p;
    for (auto* c : p.getChildren())
        if (auto* r = find(*c, id))
            return r;
    return nullptr;
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> p, bool drag = false, int modifiers = 0)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            p,
            juce::ModifierKeys::leftButtonModifier | modifiers,
            1,
            0,
            0,
            0,
            0,
            &c,
            &c,
            now,
            p,
            now,
            1,
            drag};
}
void invoke(Workspace& w, int cmd)
{
    check(w.uiCommands().invokeDirectly(cmd, false), "native ruler command registered and available");
    pump();
}
void drag(EditWindow& edit, juce::Point<float> from, juce::Point<float> to)
{
    edit.mouseDown(event(edit, from));
    edit.mouseDrag(event(edit, to, true));
    edit.mouseUp(event(edit, to, true));
    pump();
}
Json op(const char* cmd, Json args)
{
    return {{"command", cmd}, {"args", args}};
}
Json clip(const Workspace& w)
{
    const auto q = w.query();
    return q["tracks"][0]["clips"][0];
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("forma-rulers-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        const auto source = dir.getChildFile("source.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> stream = source.createOutputStream();
            auto writer = format.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(bool(writer), "real PCM source writer created");
            juce::AudioBuffer<float> pcm(2, 192000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 192000; ++i)
                    pcm.setSample(ch, i, float(.04 * std::sin(i * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
            check(writer->writeFromAudioSampleBuffer(pcm, 0, 192000), "real four-second PCM written");
        }
        const auto hash = Commands::mediaHash(source);
        c.commit(c.makePlan(
            "human", Json::array({op("track.create", {{"name", "Vocal"}, {"ref", "$v"}}),
                                  op("clip.import", {{"track", "$v"},
                                                     {"path", source.getFullPathName().toStdString()},
                                                     {"position_samples", 48000}}),
                                  op("tempo.set", {{"position_samples", 96000}, {"bpm", 60.}}),
                                  op("meter.set", {{"position_samples", 288000}, {"numerator", 3}, {"denominator", 4}}),
                                  op("marker.create", {{"name", "Bridge"}, {"position_samples", 72000}})})));
        pump();
        const auto initial = w.query();
        auto* edit = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(edit && find(w, "rulers.menu") && edit->rulerHeight() == 78 && edit->markerLaneY() == 58,
              "default native rulers retain previous track and Marker geometry");
        const auto revision = initial["revision"];
        invoke(w, 161);
        check(edit->rulerHeight() == 176 && edit->markerLaneY() == 116 && edit->rowY(0) == 176,
              "all seven actual rulers move every track row using one measured height");
        check(w.query() == initial, "ruler visibility writes only UI subtree without changing revision or Undo");
        for (const auto& entry : Rulers::entries())
        {
            juce::ApplicationCommandInfo info(entry.command);
            w.getCommandInfo(entry.command, info);
            check((info.flags & juce::ApplicationCommandInfo::isTicked) != 0,
                  "menu checkbox follows actual ruler state");
        }
        for (int fps : {24, 25, 30})
        {
            const int frame = 48000 / fps;
            check(TimelineCoordinates::frames(frame - 1, fps) == "00:00:00:00" &&
                      TimelineCoordinates::frames(frame, fps) == "00:00:00:01" &&
                      TimelineCoordinates::frames(48000 - 1, fps) == "00:00:00:" + juce::String(fps - 1),
                  "timecode uses containing frame at exact sample boundaries");
            const auto ticks = Rulers::ticks({12345, 240001, 250, 723}, "timecode", fps);
            for (const auto& tick : ticks)
                check(tick["samples"].get<int64_t>() % frame == 0, "timecode ticks align to exact frames");
        }
        const auto sampleTicks = Rulers::ticks({12345, 240001, 250, 723}, "samples", 24);
        for (const auto& tick : sampleTicks)
            check(std::stoll(tick["label"].get<std::string>()) == tick["samples"],
                  "Samples ruler label is the actual integer session position");
        rejects([] { Rulers::ticks({0, 480, 250, 500}, "timecode", 0); },
                "invalid fps rejected without division by zero");
        const auto atMeter = c.timelinePosition(c.sampleAtBeat(8));
        check(std::abs(atMeter["bpm"].get<double>() - 60) < 1e-6 && atMeter["numerator"] == 3 &&
                  atMeter["denominator"] == 4,
              "local ruler read API uses real Tempo and Meter map at requested sample");
        check(c.timelinePosition(c.sampleAtBeat(9))["bar"] == 3 && c.timelinePosition(c.sampleAtBeat(9))["beat"] == 2.,
              "Bars and Beats position follows meter changes rather than uniform BPM arithmetic");
        rejects([&] { c.timelinePosition(-1); }, "negative local position rejected");
        auto* counter = dynamic_cast<juce::Label*>(find(w, "transport.main_counter"));
        c.seek(c.sampleAtBeat(9));
        pump();
        invoke(w, 166);
        check(counter && counter->getText() == "3 | 2.000",
              "main ruler changes real counter to actual musical position");
        invoke(w, 169);
        check(counter->getText().getLargeIntValue() == c.sampleAtBeat(9),
              "Samples main counter uses exact same L1 position");
        invoke(w, 168);
        invoke(w, 164);
        check(counter->getText() == TimelineCoordinates::frames(c.sampleAtBeat(9), 25),
              "Timecode main counter and displayed fps share formatter");
        invoke(w, 162);
        check(edit->rulerHeight() == 29 && edit->markerLaneY() == -1 && w.queryView()["rulers"]["timecode"] == true,
              "None leaves actual Main Time Scale visible and hides other rulers");
        auto switches = w.queryView()["rulers"];
        switches["timecode"] = false;
        rejects([&] { c.updateUiState({{"rulers", switches}}, c.sessionToken()); },
                "cannot hide primary ruler through direct UI patch");
        invoke(w, 161);
        // Ruler-name clicks use the same command manager; Option-click hides an optional lane.
        edit->mouseDown(event(*edit, {80.f, 8.f}));
        pump();
        check(w.queryView()["main_time_scale"] == "bars_beats", "actual ruler-name click selects Main Time Scale");
        edit->mouseDown(event(*edit, {80.f, float(Rulers::top(w.queryView(), "samples") + 8)}, false,
                              juce::ModifierKeys::altModifier));
        pump();
        check(!w.queryView()["rulers"]["samples"].get<bool>() && edit->rulerHeight() == 147,
              "Option-click optional ruler name removes real row and relocates tracks");
        invoke(w, 161);
        const auto marker = w.query()["markers"][0];
        edit->mouseDown(event(*edit, {float(edit->coordinates().pixelAt(72000)), float(edit->markerLaneY() + 8)}));
        pump();
        check(w.query()["position_samples"] == 72000 && find(w, "memory.locations.panel"),
              "Marker hit testing follows shifted lane and opens actual Memory Location");
        w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        pump();
        for (const auto& entry : Rulers::entries())
            if (entry.mainCommand != 0)
            {
                const auto axis = edit->coordinates();
                const float y = float(Rulers::top(w.queryView(), entry.key) + 10);
                drag(*edit, {float(axis.pixelAt(12000)), y}, {float(axis.pixelAt(36000)), y});
                const auto selection = w.query()["time_selection"];
                const auto expectedStart = axis.sampleAt(juce::roundToInt(float(axis.pixelAt(12000))));
                const auto expectedEnd = expectedStart + std::llround((juce::roundToInt(float(axis.pixelAt(36000))) -
                                                                       juce::roundToInt(float(axis.pixelAt(12000)))) *
                                                                      axis.span / axis.width);
                std::cout << "RULER " << entry.key << " expected " << expectedStart << ".." << expectedEnd << " actual "
                          << selection.dump() << '\n';
                check(!selection.is_null() && selection["start_samples"] == expectedStart &&
                          selection["end_samples"] == expectedEnd,
                      "timebase ruler selection uses exact integer mouse pixel to sample mapping for all rows");
            }
        const auto original = clip(w);
        auto from = edit->clipRect(original, 0).getCentre().toFloat();
        const auto shift = std::llround(20 * edit->coordinates().span / edit->coordinates().width);
        drag(*edit, from, from + juce::Point<float>(20, 0));
        check(std::abs(clip(w)["start_samples"].get<int64_t>() - original["start_samples"].get<int64_t>() - shift) <= 1,
              "actual PCM clip drag uses relocated timeline sample axis");
        invoke(w, 6);
        check(clip(w) == original, "one Undo restores actual clip gesture with expanded rulers");
        // Loop edges edit a separate loop range; keep the existing Edit selection intact.
        c.commit(c.makePlan(
            "human", Json::array({op("session.range.set", {{"start_samples", 48000}, {"end_samples", 144000}}),
                                  op("transport.loop.set", {{"enabled", true}}),
                                  op("session.range.set", {{"start_samples", 12000}, {"end_samples", 36000}})})));
        pump();
        const auto beforeLoop = w.query(), loopSelection = beforeLoop["time_selection"];
        from = edit->loopHandleRect(false).getCentre().toFloat();
        drag(*edit, from, from + juce::Point<float>(30, 0));
        auto afterLoop = w.query();
        check(afterLoop["transport_settings"]["loop_range"]["end_samples"].get<int64_t>() ==
                      144000 + std::llround(30 * edit->coordinates().span / edit->coordinates().width) &&
                  afterLoop["time_selection"] == loopSelection &&
                  afterLoop["revision"] == beforeLoop["revision"].get<uint64_t>() + 1,
              "loop end handle submits one L1 transaction and preserves independent Edit selection");
        invoke(w, 6);
        check(w.query()["transport_settings"] == beforeLoop["transport_settings"],
              "single Undo restores previous native loop endpoints");
        invoke(w, 7);
        check(w.query()["transport_settings"] == afterLoop["transport_settings"],
              "Redo restores exact native loop range");
        from = edit->loopHandleRect(true).getCentre().toFloat();
        drag(*edit, from, from + juce::Point<float>(15, 0));
        check(w.query()["transport_settings"]["loop_range"]["start_samples"].get<int64_t>() ==
                  48000 + std::llround(15 * edit->coordinates().span / edit->coordinates().width),
              "start loop handle edits true sample endpoint");
        // A concurrent project edit must reject an old edge gesture.
        from = edit->loopHandleRect(false).getCentre().toFloat();
        edit->mouseDown(event(*edit, from));
        edit->mouseDrag(event(*edit, from + juce::Point<float>(10, 0), true));
        c.commit(c.makePlan("human",
                            Json::array({op("marker.rename", {{"marker", marker["id"]}, {"name", "Newer human"}})})));
        pump();
        const auto newer = w.query();
        edit->mouseUp(event(*edit, from + juce::Point<float>(10, 0), true));
        pump();
        check(w.query() == newer, "stale loop gesture does not overwrite a later human transaction");
        from = edit->clipRect(clip(w), 0).getCentre().toFloat();
        edit->mouseDown(event(*edit, from));
        edit->mouseDrag(event(*edit, from + juce::Point<float>(10, 0), true));
        const auto priorLayout = w.query();
        invoke(w, 159);
        edit->mouseUp(event(*edit, from + juce::Point<float>(10, 0), true));
        pump();
        check(w.query() == priorLayout, "ruler layout change cancels an in-flight pixel gesture without edits");
        const auto beforeBad = w.queryView();
        rejects([&] { c.updateUiState({{"rulers", {{"min_sec", true}}}}, c.sessionToken()); },
                "incomplete ruler map rejected");
        rejects([&] { c.updateUiState({{"timecode_fps", 29}}, c.sessionToken()); },
                "unsupported drop/fractional rate not misreported as supported");
        rejects([&] { c.updateUiState({{"main_time_scale", "tempo"}}, c.sessionToken()); },
                "Conductor cannot masquerade as Main Time Scale");
        check(w.queryView() == beforeBad, "bad UI preferences do not partially replace good layout");
        auto old = beforeBad;
        old["ui_schema"] = 5;
        for (const auto* key : {"rulers", "main_time_scale", "timecode_fps", "track_heights", "zoom_presets", "track_views"})
            old.erase(key);
        juce::ValueTree meta("NATIVEDAW"), ui("UI");
        ui.setProperty("json", text(old.dump()), nullptr);
        meta.addChild(ui, -1, nullptr);
        auto migrated = readUiState(meta);
        check(migrated["ui_schema"] == 8 && migrated["rulers"] == Rulers::defaults() &&
                  migrated["edit_views"] == old["edit_views"],
              "complete schema5 migrates preserving prior columns and default ruler geometry");
        old.erase("row_height");
        ui.setProperty("json", text(old.dump()), nullptr);
        rejects([&] { readUiState(meta); }, "incomplete old UI snapshot not silently guessed");
        invoke(w, 161);
        invoke(w, 168);
        invoke(w, 165);
        auto keys = w.uiCommands().getKeyMappings();
        keys->clearAllKeyPresses(159);
        const juce::KeyPress custom('u', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 'u');
        keys->addKeyPress(159, custom);
        pump();
        w.setSize(1120, 700);
        pump();
        check(edit->rulerHeight() == 176 && edit->rowY(0) == 176 &&
                  find(w, "rulers.menu")->getBounds().getRight() < edit->timelineLeft(),
              "all rulers and native selector fit minimum window with real track geometry");
        c.save(dir.getChildFile("rulers.tracktionedit"));
        const auto saved = w.query(), savedView = w.queryView();
        Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("reopened")));
        reopened.setVisible(true);
        reopened.setSize(1120, 700);
        reopened.openSession(dir.getChildFile("rulers.tracktionedit"));
        pump();
        check(reopened.query()["tracks"] == saved["tracks"] && reopened.query()["markers"] == saved["markers"] &&
                  reopened.query()["transport_settings"] == saved["transport_settings"],
              "actual new Workspace reopen preserves clips markers and native loop endpoints");
        check(reopened.queryView()["rulers"] == savedView["rulers"] &&
                  reopened.queryView()["main_time_scale"] == "timecode" && reopened.queryView()["timecode_fps"] == 30,
              "native save/reopen preserves all ruler choices and main scale/fps");
        check(reopened.uiCommands().getKeyMappings()->containsMapping(159, custom) && reopened.keyPressed(custom),
              "custom ruler shortcut survives reopen and invokes same command");
        pump();
        check(!reopened.queryView()["rulers"]["tempo"].get<bool>(), "restored keyboard binding hides exact Tempo lane");
        check(Commands::mediaHash(source) == hash && clip(reopened) == original,
              "all ruler and loop edits preserve original PCM hash and clip mapping");
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"ui_schema", 8},
                    {"scope", "real native components and Edit; closed audio device; desktop acceptance unexecuted"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("ruler report write failed");
        }
        std::cout << report.dump(2) << '\n';
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
