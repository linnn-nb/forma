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
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File path) : PropertyStorage("Forma Edit view tests"), path(path) {}
    juce::File getAppPrefsFolder() override
    {
        path.createDirectory();
        return path;
    }

private:
    juce::File path;
};
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (!p.isVisible())
        return nullptr;
    if (p.getComponentID() == id)
        return &p;
    for (auto* child : p.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> p, bool drag = false)
{
    auto now = juce::Time::getCurrentTime();
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
            drag};
}
void click(Workspace& w, const juce::String& id)
{
    auto* b = dynamic_cast<juce::Button*>(find(w, id));
    check(b && b->isEnabled(), "live native action exists and is enabled");
    b->triggerClick();
    pump();
}
Json op(const char* cmd, Json args)
{
    return {{"command", cmd}, {"args", args}};
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
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("forma-edit-views-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        auto pcmFile = dir.getChildFile("source.wav");
        {
            juce::WavAudioFormat f;
            std::unique_ptr<juce::OutputStream> stream = pcmFile.createOutputStream();
            auto writer = f.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(bool(writer), "real PCM writer available");
            juce::AudioBuffer<float> data(2, 96000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 96000; ++i)
                    data.setSample(ch, i, float(.03 * std::sin(i * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
            check(writer->writeFromAudioSampleBuffer(data, 0, 96000), "actual two-second PCM media written");
        }
        auto hash = Commands::mediaHash(pcmFile);
        c.commit(c.makePlan(
            "human",
            Json::array(
                {op("track.create", {{"name", "Vocal"}, {"ref", "$v"}}),
                 op("clip.import",
                    {{"track", "$v"}, {"path", pcmFile.getFullPathName().toStdString()}, {"position_samples", 0}}),
                 op("plugin.insert", {{"track", "$v"}, {"type", te::EqualiserPlugin::xmlTypeName}}),
                 op("track.create", {{"name", "Room"}, {"type", "aux"}, {"ref", "$a"}}),
                 op("plugin.insert", {{"track", "$a"}, {"type", "reverb"}, {"wet_only", true}}),
                 op("send.create", {{"track", "$v"}, {"target", "$a"}, {"db", -18}, {"position", "post"}}),
                 op("track.create", {{"name", "Folder"}, {"type", "folder"}, {"ref", "$folder"}}),
                 op("track.create", {{"name", "Echo"}, {"type", "aux"}, {"ref", "$echo"}}),
                 op("send.create", {{"track", "$v"}, {"target", "$echo"}, {"db", -24}, {"position", "pre"}})})));
        pump();
        auto baseline = w.query();
        auto id = baseline["tracks"][0]["id"].get<std::string>(),
             folder = baseline["tracks"][2]["id"].get<std::string>();
        auto* edit = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(edit && edit->coordinates().left == 250,
              "default timeline retains original header and sample coordinates");
        for (int cmd : {146, 147, 148})
        {
            check(w.uiCommands().invokeDirectly(cmd, false), "view command registered");
            pump();
            juce::ApplicationCommandInfo info(cmd);
            w.getCommandInfo(cmd, info);
            check((info.flags & juce::ApplicationCommandInfo::isTicked) != 0, "view menu reflects enabled state");
        }
        check(w.query() == baseline, "column display changes no audio facts revision or Undo");
        check(edit->columnCount() == 3 && edit->coordinates().left == 562,
              "timeline shares dynamic header plus three view columns");
        auto* insert = dynamic_cast<juce::TextButton*>(find(w, "edit.insert:" + text(id) + ":0"));
        auto* send = dynamic_cast<juce::TextButton*>(find(w, "edit.send:" + text(id) + ":0"));
        check(insert && insert->getButtonText() == text(baseline["tracks"][0]["plugins"][0]["name"]),
              "insert A displays actual plugin instance name");
        check(send && send->getButtonText() == "Echo" && send->getTooltip().contains("pre") &&
                  send->getTooltip().contains("-24.0"),
              "send displays actual target position and gain");
        check(!find(w, "edit.insert:" + text(folder) + ":0")->isEnabled(),
              "folder unsupported insert is disabled rather than pretending to host");
        click(w, "edit.insert:" + text(id) + ":0");
        check(w.query() == baseline && find(w, "plugin.choice"),
              "existing Edit insert opens actual processor inspector without changing facts");
        click(w, "edit.input:" + text(id));
        check(find(w, "track.arm") && w.query() == baseline, "input column opens native recording inspector");
        click(w, "edit.send:" + text(id) + ":1");
        auto* secondLevel = dynamic_cast<juce::Slider*>(find(w, "send.level"));
        check(secondLevel && std::abs(secondLevel->getValue() + 18) < .02 &&
                  dynamic_cast<juce::ComboBox*>(find(w, "send.choice"))->getSelectedId() == 2,
              "second send slot selects exact stable instance rather than first send");
        click(w, "edit.send:" + text(id) + ":0");
        auto* level = dynamic_cast<juce::Slider*>(find(w, "send.level"));
        check(level && std::abs(level->getValue() + 24) < .02, "send column opens real routing controls");
        level->setValue(-9, juce::sendNotificationSync);
        pump();
        check(std::abs(w.query()["tracks"][0]["sends"][0]["db"].get<double>() + 9) < .02,
              "column-routed gesture commits actual send level through L1");
        check(send->getTooltip().contains("-9.0"), "column live-refresh follows committed send gain");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(std::abs(w.query()["tracks"][0]["sends"][0]["db"].get<double>() + 24) < .02,
              "one Undo restores real send level");
        w.uiCommands().invokeDirectly(7, false);
        pump();
        check(std::abs(w.query()["tracks"][0]["sends"][0]["db"].get<double>() + 9) < .02,
              "Redo restores same send level and identity");
        const auto originalClip = w.query()["tracks"][0]["clips"][0];
        auto point = edit->clipRect(originalClip, 0).getCentre().toFloat();
        const auto expected = std::llround(20 * edit->coordinates().span / edit->coordinates().width);
        edit->mouseDown(event(*edit, point));
        edit->mouseDrag(event(*edit, point + juce::Point<float>(20, 0), true));
        edit->mouseUp(event(*edit, point + juce::Point<float>(20, 0), true));
        pump();
        check(std::abs(w.query()["tracks"][0]["clips"][0]["start_samples"].get<int64_t>() - expected) <= 1,
              "audio drag uses adjusted sample/pixel axis with all view columns open");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(w.query()["tracks"][0]["clips"][0]["start_samples"] == 0, "one Undo restores column-layout clip drag");
        w.uiCommands().invokeDirectly(7, false);
        pump();
        check(std::abs(w.query()["tracks"][0]["clips"][0]["start_samples"].get<int64_t>() - expected) <= 1,
              "Redo preserves actual clip identity and adjusted position");
        const auto selection = w.query()["tracks"][0]["clips"][0];
        click(w, "clip.select:" + text(selection["id"]));
        auto* clips = dynamic_cast<juce::ListBox*>(find(w, "clips.list")->getChildComponent(0));
        check(clips && clips->getSelectedRow() == 0, "Clips sidebar highlights canonical timeline clip selection");
        w.setSize(1120, 700);
        pump();
        check(edit->coordinates().width >= 160, "all columns preserve timeline width at minimum supported window size");
        for (auto name : {"edit.input:", "edit.output:", "edit.insert:", "edit.send:"})
        {
            auto key = juce::String(name) + text(id) +
                       (juce::String(name).contains("insert") || juce::String(name).contains("send") ? ":4" : "");
            auto* b = find(w, key);
            check(b && b->getParentComponent()->getLocalBounds().contains(b->getBounds()),
                  "enabled row controls stay inside view region at minimum window size");
        }
        check(!find(w, "audio.settings.open"), "audio configuration remains in menu without overlapping edit toolbar");
        const auto view = w.queryView(), facts = w.query();
        auto file = dir.getChildFile("views.tracktionedit");
        c.save(file);
        Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("reopen")));
        reopened.setVisible(true);
        reopened.setSize(1120, 700);
        reopened.openSession(file);
        pump();
        check(reopened.queryView()["edit_views"] == view["edit_views"] && reopened.query()["tracks"] == facts["tracks"],
              "actual new workspace reopen preserves columns and media route plugin and send facts");
        auto* restored = dynamic_cast<EditWindow*>(find(reopened, "edit.timeline"));
        check(restored && restored->columnCount() == 3, "reopened native layout matches saved column switches");
        check(reopened.keyPressed(
                  juce::KeyPress('2', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, '2')),
              "view default shortcut invokes unified command");
        pump();
        check(!reopened.queryView()["edit_views"]["inserts"].get<bool>() && restored->columnCount() == 2,
              "customizable key hides only requested column");
        juce::ValueTree meta("NATIVEDAW"), ui("UI");
        auto old = view;
        old.erase("edit_views");
        old["ui_schema"] = 3;
        ui.setProperty("json", juce::String(old.dump()), nullptr);
        meta.addChild(ui, -1, nullptr);
        auto migrated = readUiState(meta);
        check(migrated["ui_schema"] == 5 &&
                  migrated["edit_views"] ==
                      Json({{"io", false}, {"inserts", false}, {"sends", false}, {"comments", false}}) &&
                  migrated["object_selection"] == old["object_selection"],
              "complete prior schema3 migrates preserving shared selection");
        old.erase("midi_scroll_y");
        ui.setProperty("json", juce::String(old.dump()), nullptr);
        rejects([&] { readUiState(meta); }, "incomplete prior UI is not silently defaulted");
        rejects([&] { c.updateUiState({{"edit_views", {{"io", true}}}}, c.sessionToken()); },
                "partial column map rejected atomically");
        rejects(
            [&]
            { c.updateUiState({{"edit_views", {{"io", true}, {"inserts", true}, {"sends", 1}}}}, c.sessionToken()); },
            "nonboolean view permission-like data rejected");
        check(Commands::mediaHash(pcmFile) == hash, "UI operations and send editing preserve source media bytes");
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"scope", "native Edit views real plugin/send/input routing controls Undo schema migration and "
                              "reopen; closed device"}};
        if (argc > 1)
        {
            std::ofstream f(argv[1]);
            f << report.dump(2);
            if (!f)
                throw std::runtime_error("result write failed");
        }
        std::cout << report.dump(2) << std::endl;
        dir.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
