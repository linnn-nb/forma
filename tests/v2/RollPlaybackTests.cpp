#include "Workspace.h"
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
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
    static te::Engine& engine(Commands& c)
    {
        return c.engine;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
void check(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
    ++checks;
    std::cout << "PASS " << message << std::endl;
}
void pump(int ms = 80)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File dir) : PropertyStorage("Forma roll tests"), dir(dir) {}
    juce::File getAppPrefsFolder() override
    {
        dir.createDirectory();
        return dir;
    }

private:
    juce::File dir;
};
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (!p.isVisible())
        return nullptr;
    if (p.getComponentID() == id)
        return &p;
    for (auto* c : p.getChildren())
        if (auto* found = find(*c, id))
            return found;
    return nullptr;
}
void invoke(Workspace& w, int id)
{
    check(w.uiCommands().invokeDirectly(id, false), "native roll command available");
    pump();
}
template <class F> void reject(F f, const char* why)
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
juce::MouseEvent event(juce::Component& c, juce::Point<float> p)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            p,
            juce::ModifierKeys::leftButtonModifier,
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
            false};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("forma-roll-" + juce::Uuid().toString());
        dir.createDirectory();
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        const auto source = dir.getChildFile("zones.wav");
        {
            juce::WavAudioFormat f;
            std::unique_ptr<juce::OutputStream> out = source.createOutputStream();
            auto writer = f.createWriterFor(
                out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(writer != nullptr, "real WAV writer available");
            juce::AudioBuffer<float> pcm(2, 144000);
            for (int i = 0; i < 144000; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    pcm.setSample(ch, i, i < 24000 ? .03f : i < 48000 ? .05f : i < 72000 ? .1f : i < 96000 ? .2f : .3f);
            check(writer->writeFromAudioSampleBuffer(pcm, 0, 144000), "real pre selection post zones written");
        }
        const auto hash = juce::SHA256(source).toHexString().toStdString();
        c.commit(c.makePlan(
            "human",
            Json::array({operation("track.create", {{"name", "Roll verification"}, {"ref", "$a"}}),
                         operation("clip.import", {{"track", "$a"},
                                                   {"path", source.getFullPathName().toStdString()},
                                                   {"position_samples", 0}}),
                         operation("session.range.set", {{"start_samples", 48000}, {"end_samples", 72000}})})));
        const auto initial = c.query()["transport_settings"]["roll"];
        Json roll{{"pre_enabled", true}, {"post_enabled", true}, {"pre_samples", 24000}, {"post_samples", 24000}};
        const auto plan = c.makePlan("human", Json::array({operation("transport.roll.set", roll)}));
        check(c.preview(plan)["transport_changes"][0]["before"] == initial, "preview reports actual roll settings");
        c.commit(plan);
        check(c.query()["transport_settings"]["roll"] == roll,
              "actual roll settings committed in one native transaction");
        c.undo();
        check(c.query()["transport_settings"]["roll"] == initial, "Undo restores all roll fields");
        c.redo();
        check(c.query()["transport_settings"]["roll"] == roll, "Redo restores fields");
        auto invalid = roll;
        invalid["pre_samples"] = -1;
        reject([&] { c.commit(c.makePlan("human", Json::array({operation("transport.roll.set", invalid)}))); },
               "negative duration rejected atomically");
        reject([&] { c.commit(c.makePlan("agent:external", Json::array({operation("transport.roll.set", roll)}))); },
               "frozen Agent tool path does not gain ruler edit permissions");
        for (const auto& unit : {"samples", "min_sec", "bars_beats"})
            for (int64_t anchor : {int64_t(0), int64_t(48000), int64_t(72000)})
                for (int64_t length : {int64_t(0), int64_t(37), int64_t(12001), int64_t(96000), int64_t(3600048)})
                    for (bool pre : {true, false})
                        check(c.parseRollDuration(c.formatRollDuration(length, anchor, pre, unit, 24), anchor, pre,
                                                  unit, 24) == length,
                              "sample/seconds/native-beat duration preserves exact samples including pre-zero");
        for (int fps : {24, 25, 30})
        {
            const int64_t frame = 48000 / fps;
            check(c.parseRollDuration("00:01:02:03", 48000, false, "timecode", fps) == 62 * 48000 + 3 * frame,
                  "actual NDF frame rate determines duration");
            check(c.formatRollDuration(frame * 17 + 1, 48000, true, "timecode", fps) == "00:00:00:17",
                  "timecode display reports containing frame rather than changing sub-frame samples");
            reject([&] { c.parseRollDuration("00:00:00:" + std::to_string(fps), 0, false, "timecode", fps); },
                   "out-of-range frame rejected");
        }
        for (const auto& invalidText : {"nan", "-1", "1e6", "00:60", "1:2:3", "0.25bad", "999999999999"})
            reject([&] { c.parseRollDuration(invalidText, 48000, true, "min_sec", 24); },
                   "invalid seconds syntax or timeline overflow rejected");
        reject([&] { c.parseRollDuration("9223372036854775808", 0, true, "samples", 24); },
               "sample integer overflow rejected");
        reject([&] { c.parseRollDuration("1", 0, true, "min_sec", 29); }, "unsupported frame-rate context rejected");
        c.commit(
            c.makePlan("human", Json::array({operation("tempo.event.create", {{"beat_position", 4.}, {"bpm", 60.}})})));
        const auto anchor = c.sampleAtBeat(6.);
        check(c.parseRollDuration("4", anchor, true, "bars_beats", 24) == 144000 &&
                  c.parseRollDuration("4", c.sampleAtBeat(2.), false, "bars_beats", 24) == 144000,
              "pre and post duration cross actual Tempo change instead of fixed BPM multiplication");
        check(c.formatRollDuration(144000, anchor, true, "bars_beats", 24) == "4.000000000",
              "native Tempo map duration displays the inverse musical length");
        c.undo();
        c.commit(c.makePlan("human",
                            Json::array({operation("meter.event.create",
                                                   {{"beat_position", 4.}, {"numerator", 3}, {"denominator", 8}})})));
        check(c.parseRollDuration("2", c.sampleAtBeat(4.), false, "bars_beats", 24) ==
                  c.sampleAtBeat(6.) - c.sampleAtBeat(4.),
              "musical duration follows native Meter-dependent beat length");
        c.undo();
        pump();
        invoke(w, 279);
        auto input = [&](const char* id, const char* value)
        {
            auto* t = dynamic_cast<juce::TextEditor*>(find(w, id));
            check(t != nullptr, "real roll input visible");
            t->setText(value, false);
        };
        input("transport.roll.pre_samples", "0.25");
        invoke(w, 275);
        check(c.query()["transport_settings"]["roll"]["pre_samples"] == 12000,
              "native panel commits actual pre-roll duration");
        c.undo();
        pump();
        invoke(w, 279);
        input("transport.roll.pre_samples", "12bad");
        const auto before = c.query();
        invoke(w, 275);
        check(c.query() == before && find(w, "transport.roll.panel"),
              "invalid numeric text preserves panel and actual facts");
        invoke(w, 277);
        invoke(w, 279);
        input("transport.roll.pre_samples", "0.270833333");
        const auto track = c.query()["tracks"][0]["id"];
        c.commit(c.makePlan("human", Json::array({operation("track.gain", {{"track", track}, {"db", -1.}})})));
        invoke(w, 275);
        check(c.query()["transport_settings"]["roll"] == roll && find(w, "transport.roll.panel"),
              "stale roll draft cannot overwrite interleaved human edit");
        invoke(w, 277);
        c.undo();
        pump();
        const auto originalView = c.uiState();
        auto timecodeRulers = originalView["rulers"];
        timecodeRulers["timecode"] = true;
        c.updateUiState({{"main_time_scale", "timecode"}, {"timecode_fps", 25}, {"rulers", timecodeRulers}},
                        c.sessionToken());
        auto subFrame = roll;
        subFrame["pre_samples"] = 12001;
        c.commit(c.makePlan("human", Json::array({operation("transport.roll.set", subFrame)})));
        pump();
        invoke(w, 279);
        auto* preEditor = dynamic_cast<juce::TextEditor*>(find(w, "transport.roll.pre_samples"));
        check(preEditor && preEditor->getText() == "00:00:00:06", "real panel uses current main ruler and 25fps NDF");
        auto* preToggle = dynamic_cast<juce::ToggleButton*>(find(w, "transport.roll.pre"));
        check(preToggle != nullptr, "actual pre-roll toggle available");
        preToggle->setToggleState(false, juce::dontSendNotification);
        input("transport.roll.post_samples", "00:00:00:07");
        invoke(w, 275);
        check(c.query()["transport_settings"]["roll"]["pre_samples"] == 12001 &&
                  c.query()["transport_settings"]["roll"]["post_samples"] == 13440 &&
                  !c.query()["transport_settings"]["roll"]["pre_enabled"].get<bool>(),
              "checkbox preserves exact sub-frame pre length while edited post becomes seven native frames");
        const auto unitFile = dir.getChildFile("RollUnits.tracktionedit");
        c.save(unitFile);
        {
            Workspace restored(false, std::make_unique<Storage>(dir.getChildFile("unit-reopen")));
            restored.setVisible(true);
            restored.setSize(1600, 1000);
            auto& owner = AudioDeviceTestAccess::owner(restored);
            owner.open(unitFile);
            pump();
            check(owner.query()["transport_settings"]["roll"] == c.query()["transport_settings"]["roll"] &&
                      owner.uiState()["main_time_scale"] == "timecode" && owner.uiState()["timecode_fps"] == 25,
                  "native close reopen preserves frame-edited post and exact disabled sub-frame pre");
            invoke(restored, 279);
            auto* restoredInput = dynamic_cast<juce::TextEditor*>(find(restored, "transport.roll.pre_samples"));
            check(restoredInput && restoredInput->getText() == "00:00:00:06",
                  "reopened panel displays restored main ruler frame format");
        }
        c.undo();
        check(c.query()["transport_settings"]["roll"] == subFrame,
              "one native Undo restores unit-entry and checkbox together");
        c.redo();
        check(c.query()["transport_settings"]["roll"]["pre_samples"] == 12001, "Redo retains unquantised duration");
        c.undo();
        c.undo();
        c.updateUiState({{"main_time_scale", "bars_beats"}}, c.sessionToken());
        pump();
        invoke(w, 279);
        input("transport.roll.pre_samples", "0.5");
        invoke(w, 275);
        check(c.query()["transport_settings"]["roll"]["pre_samples"] == 12000,
              "real main Bars Beats panel submits musical duration through L1");
        c.undo();
        c.updateUiState({{"main_time_scale", originalView["main_time_scale"]},
                         {"timecode_fps", originalView["timecode_fps"]},
                         {"rulers", originalView["rulers"]}},
                        c.sessionToken());
        pump();
        auto* edit = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(edit != nullptr, "actual timeline visible");
        const auto point = RollRuler::flag(c.query(), c.uiState(), edit->coordinates(), true).getCentre().toFloat();
        check(point.x > edit->coordinates().left, "actual pre-roll flag located on main ruler");
        auto disabled = roll;
        disabled["pre_enabled"] = false;
        disabled["post_enabled"] = false;
        c.commit(c.makePlan("human", Json::array({operation("transport.roll.set", disabled)})));
        pump();
        const auto grayFlag = RollRuler::flag(c.query(), c.uiState(), edit->coordinates(), true);
        check(!grayFlag.isEmpty() && grayFlag.getCentre().toFloat() == point,
              "disabled pre-roll retains stored position and hit target on main ruler");
        juce::Image flagImage(juce::Image::ARGB, edit->getWidth(), Rulers::height(c.uiState()), true);
        juce::Graphics graphics(flagImage);
        RollRuler painter;
        painter.paint(graphics, c.query(), c.uiState(), edit->coordinates());
        check(flagImage.getPixelAt(grayFlag.getX() + 7, grayFlag.getY() + 2) == juce::Colour(0xff697785),
              "production ruler paints disabled flag gray from actual saved facts");
        const auto grayDestination = juce::Point<float>{float(edit->coordinates().pixelAt(36000)), point.y};
        edit->mouseDown(event(*edit, point));
        edit->mouseDrag(event(*edit, grayDestination));
        check(c.query()["transport_settings"]["roll"] == disabled,
              "disabled flag drag leaves facts untouched until release");
        edit->mouseUp(event(*edit, grayDestination));
        pump();
        check(!c.query()["transport_settings"]["roll"]["pre_enabled"].get<bool>() &&
                  std::abs(c.query()["transport_settings"]["roll"]["pre_samples"].get<int64_t>() - 12000) < 400,
              "gray flag moves duration without silently enabling playback roll");
        c.undo();
        check(c.query()["transport_settings"]["roll"] == disabled, "gray flag Undo restores disabled settings");
        c.undo();
        pump();
        edit->mouseDown(event(*edit, point));
        const auto destination = juce::Point<float>{float(edit->coordinates().pixelAt(36000)), point.y};
        edit->mouseDrag(event(*edit, destination));
        check(c.query()["transport_settings"]["roll"] == roll, "flag drag remains local draft before release");
        edit->mouseUp(event(*edit, destination));
        pump();
        check(std::abs(c.query()["transport_settings"]["roll"]["pre_samples"].get<int64_t>() - 12000) < 400,
              "flag release commits duration from shared sample geometry");
        c.undo();
        pump();
        check(c.query()["transport_settings"]["roll"] == roll, "flag edit Undo restores native state");
        const auto revision = c.query()["revision"];
        edit->mouseDown(event(*edit, point));
        edit->mouseDrag(event(*edit, destination));
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)), "Escape cancels flag draft");
        edit->mouseUp(event(*edit, destination));
        check(c.query()["revision"] == revision, "cancel does not create project transaction");
        const auto postPoint =
            RollRuler::flag(c.query(), c.uiState(), edit->coordinates(), false).getCentre().toFloat();
        const auto postTarget = juce::Point<float>{float(edit->coordinates().pixelAt(84000)), postPoint.y};
        edit->mouseDown(event(*edit, postPoint));
        edit->mouseDrag(event(*edit, postTarget));
        edit->mouseUp(event(*edit, postTarget));
        pump();
        check(std::abs(c.query()["transport_settings"]["roll"]["post_samples"].get<int64_t>() - 12000) < 400 &&
                  c.query()["transport_settings"]["roll"]["pre_samples"] == 24000,
              "post flag uses shared geometry without changing pre-roll");
        c.undo();
        pump();
        edit->mouseDown(event(*edit, point));
        edit->mouseDrag(event(*edit, destination));
        c.commit(c.makePlan("human", Json::array({operation("track.gain", {{"track", track}, {"db", -2.}})})));
        pump();
        const auto changed = c.query();
        edit->mouseUp(event(*edit, destination));
        pump();
        check(c.query() == changed, "revision change cancels stale ruler draft without overwriting human edit");
        c.undo();
        pump();
        const auto view = c.uiState();
        edit->mouseDown(event(*edit, point));
        edit->mouseDrag(event(*edit, destination));
        c.updateUiState({{"span_samples", 240000}}, c.sessionToken());
        pump();
        const auto zoomed = c.query();
        edit->mouseUp(event(*edit, destination));
        pump();
        check(c.query() == zoomed, "coordinate change cancels stale flag draft");
        c.updateUiState({{"span_samples", view["span_samples"]}}, c.sessionToken());
        pump();
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress toggle('k', juce::ModifierKeys::commandModifier, 0);
        check(keys->containsMapping(278, toggle) && keys->keyPressed(toggle, &w),
              "default Command K invokes actual roll toggle");
        pump();
        check(!c.query()["transport_settings"]["roll"]["pre_enabled"].get<bool>() &&
                  !c.query()["transport_settings"]["roll"]["post_enabled"].get<bool>(),
              "native keyboard toggle disables both fields in one transaction");
        c.undo();
        pump();
        const juce::KeyPress custom('r', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        keys->clearAllKeyPresses(279);
        keys->addKeyPress(279, custom);
        pump();
        const auto file = dir.getChildFile("Roll.tracktionedit");
        c.save(file);
        {
            Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("reopen")));
            reopened.setVisible(true);
            reopened.setSize(1600, 1000);
            auto& owner = AudioDeviceTestAccess::owner(reopened);
            owner.open(file);
            pump();
            check(owner.query()["transport_settings"]["roll"] == roll &&
                      owner.query()["time_selection"] == c.query()["time_selection"],
                  "save reopen restores actual roll settings and selected range");
            check(reopened.uiCommands().getKeyMappings()->containsMapping(279, custom) &&
                      reopened.uiCommands().getKeyMappings()->keyPressed(custom, &reopened),
                  "custom roll shortcut restores and executes");
            pump();
            check(find(reopened, "transport.roll.panel") != nullptr, "restored shortcut opens native roll panel");
        }
        auto* rollButton = find(w, "transport.roll");
        check(rollButton != nullptr, "native roll toolbar entry visible at full workspace width");
        bool overlaps = false;
        for (auto* sibling : rollButton->getParentComponent()->getChildren())
            if (sibling != rollButton && sibling->isVisible() &&
                rollButton->getBounds().intersects(sibling->getBounds()))
                overlaps = true;
        check(!overlaps, "roll toolbar entry does not overlap existing visible controls");
        w.setSize(1120, 700);
        pump();
        invoke(w, 279);
        check(find(w, "transport.roll.pre_samples") && find(w, "ui.command:275"),
              "minimum workspace keeps roll settings and commit accessible through native command");
        invoke(w, 277);
        w.setSize(1600, 1000);
        pump();
        te::HostedAudioDeviceInterface::Parameters parameters;
        parameters.sampleRate = 48000;
        parameters.blockSize = 256;
        parameters.inputChannels = 0;
        parameters.outputChannels = 2;
        double prePeak = 0, postPeak = 0;
        int64_t actualStop = 0;
        {
            te::test_utilities::EnginePlayer player(AudioDeviceTestAccess::engine(c), parameters);
            c.seek(0);
            c.play();
            pump(5);
            auto status = c.query()["transport_settings"]["roll_playback"];
            check(status["start_samples"] == 24000 && status["end_samples"] == 96000 && c.query()["playing"],
                  "real native transport starts declared pre selection post range");
            check(status["state"] == "requested",
                  "starting range playback does not claim observed output before a callback");
            reject([&] { c.commit(c.makePlan("human", Json::array({operation("transport.roll.set", initial)}))); },
                   "roll settings cannot change during real playback");
            for (int block = 0; block < 500 && c.query()["playing"].get<bool>(); ++block)
            {
                auto pcm = player.process(256);
                const auto position = c.query()["position_samples"].get<int64_t>();
                if (position > 26000 && position < 45000)
                    prePeak = std::max(prePeak, double(pcm.getMagnitude(0, 0, pcm.getNumSamples())));
                if (position > 75000 && position < 93000)
                    postPeak = std::max(postPeak, double(pcm.getMagnitude(0, 0, pcm.getNumSamples())));
                pump(6);
            }
            pump(100);
            actualStop = c.query()["position_samples"];
            check(!c.query()["playing"].get<bool>() &&
                      c.query()["transport_settings"]["roll_playback"]["state"] == "stopped",
                  "native section stop produces observed L1 stop receipt");
            check(c.query()["transport_settings"]["roll_playback"]["actual_stop_samples"] == actualStop,
                  "range stop receipt reports actual native endpoint rather than planned value");
            check(std::abs(prePeak - .05) < .002 && std::abs(postPeak - .2) < .002,
                  "actual device output contains declared pre-roll and post-roll source zones");
            check(actualStop >= 96000 && actualStop < 106000,
                  "hosted native timed stop overshoot measured within explicit 10000-session-sample budget");
            c.seek(0);
            c.play();
            pump(10);
            c.stop();
            check(c.query()["transport_settings"]["roll_playback"]["state"] == "cancelled" &&
                      !c.query()["playing"].get<bool>(),
                  "manual stop immediately cancels range playback receipt");
            c.play();
            pump(10);
            c.seek(50000);
            check(!c.query()["playing"].get<bool>() && c.query()["position_samples"] == 50000,
                  "manual seek cancels bounded audition without stale section timer");
            c.commit(c.makePlan("human", Json::array({operation("transport.loop.set", {{"enabled", true}})})));
            c.play();
            pump(10);
            check(c.query()["transport_settings"]["roll_playback"].is_null() &&
                      c.query()["transport_settings"]["loop_enabled"],
                  "native loop mode keeps precedence over roll playback");
            c.stop();
            c.undo();
            c.commit(c.makePlan("human", Json::array({operation("transport.roll.set", initial)})));
            c.play();
            pump(10);
            check(c.query()["transport_settings"]["roll_playback"]["start_samples"] == 48000 &&
                      c.query()["transport_settings"]["roll_playback"]["end_samples"] == 72000,
                  "ordinary selection playback excludes disabled rolls");
            c.stop();
            c.undo();
            auto longer = roll;
            longer["pre_samples"] = 80000;
            c.commit(c.makePlan("human", Json::array({operation("transport.roll.set", longer)})));
            c.play();
            pump(10);
            check(c.query()["transport_settings"]["roll_playback"]["start_samples"] == 0,
                  "pre-roll clamps at actual session start");
            c.stop();
            c.undo();
            c.commit(c.makePlan("human", Json::array({operation("session.range.clear", Json::object())})));
            reject([&] { c.play(); },
                   "enabled roll playback without range rejected before any successful play receipt");
            check(!c.query()["playing"].get<bool>(), "unavailable range does not start audio playback");
            c.undo();
            c.play();
            pump(2200);
            check(!c.query()["playing"].get<bool>() &&
                      c.query()["transport_settings"]["roll_playback"]["state"] == "failed",
                  "withheld real hosted output callbacks trigger explicit two-second watchdog failure");
        }
        const auto bad = dir.getChildFile("BadRoll.tracktionedit");
        auto xml = juce::parseXML(file);
        check(xml != nullptr, "real saved native XML readable");
        std::function<void(juce::XmlElement&)> corrupt = [&](juce::XmlElement& e)
        {
            if (e.hasTagName("ROLL"))
                e.setAttribute("pre_samples", "bad");
            for (auto* child : e.getChildIterator())
                corrupt(*child);
        };
        corrupt(*xml);
        check(xml->writeTo(bad), "owned invalid roll fixture written");
        const auto current = c.query();
        reject([&] { c.open(bad); }, "invalid saved roll duration rejects reopen");
        check(c.query() == current, "rejected reopen preserves current session facts");
        check(juce::SHA256(source).toHexString().toStdString() == hash,
              "source media hash preserved after playback editing save reopen");
        Json report{{"checks", checks},
                    {"failures", 0},
                    {"test_directory", dir.getFullPathName().toStdString()},
                    {"source_sha256", hash},
                    {"pre_zone_peak", prePeak},
                    {"post_zone_peak", postPeak},
                    {"requested_end_samples", 96000},
                    {"actual_stop_samples", actualStop},
                    {"stop_overshoot_samples", actualStop - 96000},
                    {"qualification", "actual Tracktion engine hosted output; physical GUI and audio device not "
                                      "executed; native timer stop not sample-accurate"}};
        if (argc > 1)
            std::ofstream(argv[1]) << report.dump(2) << '\n';
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
