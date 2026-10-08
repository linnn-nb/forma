#include "Workspace.h"
#include "TimelineState.h"
#include <nativedaw/v2/McpSession.h>
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
    explicit Storage(juce::File dir) : PropertyStorage("Forma Comments tests"), dir(dir) {}
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
        if (auto* r = find(*c, id))
            return r;
    return nullptr;
}
void click(Workspace& w, const juce::String& id)
{
    auto* b = dynamic_cast<juce::Button*>(find(w, id));
    check(b && b->isEnabled(), "native Comments action available");
    b->triggerClick();
    pump();
}
juce::TextEditor& editor(Workspace& w)
{
    auto* e = dynamic_cast<juce::TextEditor*>(find(w, "track.comments.text"));
    if (!e)
        throw std::runtime_error("native Comments editor missing");
    return *e;
}
Json op(const char* cmd, Json args)
{
    return {{"command", cmd}, {"args", args}};
}
Json track(const Workspace& w, const std::string& id)
{
    const auto facts = w.query();
    for (const auto& t : facts["tracks"])
        if (t["id"] == id)
            return t;
    throw std::runtime_error("track missing");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("forma-comments-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        Json create = Json::array();
        for (const auto* kind : {"audio", "midi", "instrument", "aux", "folder", "vca"})
            create.push_back(op("track.create", {{"name", kind}, {"type", kind}, {"ref", "$" + std::string(kind)}}));
        c.commit(c.makePlan("human", create));
        pump();
        const auto baseline = w.query();
        std::vector<std::string> ids;
        for (const auto& t : baseline["tracks"])
        {
            ids.push_back(t["id"]);
            check(t["comment"] == "", "every real domain track starts with empty Comments");
        }
        const auto id = ids[0];
        const auto source = dir.getChildFile("source.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> stream = source.createOutputStream();
            auto pcmWriter = format.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(bool(pcmWriter), "actual source PCM writer created");
            juce::AudioBuffer<float> audio(2, 4800);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4800; ++i)
                    audio.setSample(ch, i,
                                    float(.05 * std::sin(i * 2 * juce::MathConstants<double>::pi * 440 / 48000)));
            check(pcmWriter->writeFromAudioSampleBuffer(audio, 0, 4800), "real source PCM written");
        }
        const auto mediaHash = Commands::mediaHash(source);
        c.commit(c.makePlan("human", Json::array({op("clip.import", {{"track", id},
                                                                     {"path", source.getFullPathName().toStdString()},
                                                                     {"position_samples", 0}})})));
        pump();
        for (const auto& tool : McpSession::tools(Commands::registry()))
            if (tool.at("name") == "plan.track.comment")
                throw std::runtime_error("frozen MCP expanded");
        check(true, "Comments does not expand frozen MCP tools");
        rejects([&] { c.makePlan("agent:test", Json::array({op("track.comment", {{"track", id}, {"value", "x"}})})); },
                "external actor cannot bypass local Comments command boundary");
        auto changed = c.makePlan("human", Json::array({op("track.comment", {{"track", id}, {"value", "one"}}),
                                                        op("track.comment", {{"track", id}, {"value", "two"}})}));
        const auto diff = c.preview(changed);
        check(diff["track_changes"][0]["before"] == "" && diff["track_changes"][1]["before"] == "one",
              "second operation preview uses first operation simulated state");
        check(diff.dump().find("one") != std::string::npos && diff.dump().find("two") != std::string::npos,
              "ordered Plan preview retains actual before and after comment values");
        c.commit(changed);
        pump();
        check(track(w, id)["comment"] == "two", "ordered Plan applies last value");
        c.undo();
        pump();
        check(track(w, id)["comment"] == "", "single Undo reverts whole multi-operation Plan");
        c.redo();
        pump();
        check(track(w, id)["comment"] == "two", "Redo restores same stable track comment");
        c.undo();
        pump();
        const auto q = w.query();
        check(w.uiCommands().invokeDirectly(152, false), "Edit Comments view command exists");
        pump();
        check(w.query() == q && w.queryView()["edit_views"]["comments"] == true,
              "Comments view is independent UI state with no domain history");
        auto* button = dynamic_cast<juce::TextButton*>(find(w, "edit.comment:" + text(id)));
        check(button && button->getButtonText() == text("添加备注…"),
              "actual empty Edit column offers editable Comments");
        click(w, "edit.comment:" + text(id));
        const std::string value = "主唱 · Take 02\n保留自然呼吸\nMic: U87 / 48 kHz";
        editor(w).setText(text(value), false);
        pump();
        pump();
        check(editor(w).getText().toStdString() == value && w.query() == q,
              "timer refresh preserves Unicode multiline local draft without modifying Edit");
        check(!w.keyPressed(juce::KeyPress('e', juce::ModifierKeys::commandModifier, 'e')) && w.query() == q,
              "typing overlay does not forward timeline shortcuts");
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0)),
              "Command Return applies through native panel action");
        pump();
        check(!find(w, "track.comments.panel") && track(w, id)["comment"] == value,
              "successful L1 receipt closes panel and updates actual Edit text");
        button = dynamic_cast<juce::TextButton*>(find(w, "edit.comment:" + text(id)));
        check(button->getButtonText() == text("主唱 · Take 02") && button->getTooltip() == text(value),
              "Edit displays real first line with full multiline tooltip");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(track(w, id)["comment"] == "", "GUI comment Undo restores empty value");
        w.uiCommands().invokeDirectly(7, false);
        pump();
        check(track(w, id)["comment"] == value, "GUI Redo restores Unicode comment");
        click(w, "track.select:" + text(id));
        w.uiCommands().invokeDirectly(9, false);
        pump();
        auto* mix = dynamic_cast<juce::TextButton*>(find(w, "mix.comment:" + text(id)));
        check(mix && mix->getTooltip() == text(value) && mix->getButtonText() == text("主唱 · Take 02"),
              "Mix channel reads same actual comment as Edit");
        click(w, "mix.comment:" + text(id));
        editor(w).setText("", false);
        click(w, "track.comments.apply");
        check(track(w, id)["comment"] == "", "empty text clears through L1");
        c.undo();
        pump();
        check(track(w, id)["comment"] == value, "clear is reversible");
        w.uiCommands().invokeDirectly(153, false);
        pump();
        editor(w).setText("cancel", false);
        const auto beforeCancel = w.query();
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)), "Escape closes native draft");
        pump();
        check(!find(w, "track.comments.panel") && w.query() == beforeCancel, "cancel creates no revision or Undo");
        w.uiCommands().invokeDirectly(153, false);
        pump();
        editor(w).setText("stale", false);
        c.commit(c.makePlan("human", Json::array({op("track.rename", {{"track", id}, {"name", "Vocal renamed"}})})));
        pump();
        const auto newer = w.query();
        click(w, "track.comments.apply");
        auto* error = dynamic_cast<juce::Label*>(find(w, "track.comments.status"));
        check(find(w, "track.comments.panel") && error && error->getText().contains("revision") && w.query() == newer,
              "stale draft stays visible with honest conflict and preserves newer human edit");
        click(w, "track.comments.cancel");
        for (const auto& target : ids)
            c.commit(c.makePlan("human", Json::array({op("track.comment", {{"track", target}, {"value", value}})})));
        pump();
        for (const auto& target : ids)
            check(track(w, target)["comment"] == value, "all six track types retain Comments");
        const auto boundary =
            c.makePlan("human", Json::array({op("track.comment", {{"track", id}, {"value", std::string(4096, 'x')}})}));
        check(c.preview(boundary)["track_changes"][0]["after"].get<std::string>().size() == 4096,
              "documented character boundary is valid without changing Edit");
        const auto beforeInvalid = w.query();
        Scope narrow;
        narrow.mode = Permission::Preview;
        narrow.targets = {ids[1]};
        auto denied = c.makePlan("human", Json::array({op("track.comment", {{"track", id}, {"value", "denied"}})}));
        rejects([&] { c.commit(denied, true, narrow); }, "comment edit cannot bypass real track scope");
        for (const auto& invalid :
             {std::string(4097, 'x'), std::string(16385, 'x'), std::string("a\0b", 3), std::string("\xff", 1)})
            rejects([&]
                    { c.makePlan("human", Json::array({op("track.comment", {{"track", id}, {"value", invalid}})})); },
                    "oversize NUL and invalid UTF-8 rejected before Edit mutation");
        rejects([&]
                { c.makePlan("human", Json::array({op("track.comment", {{"track", "missing"}, {"value", "x"}})})); },
                "missing track rejected with no phantom result");
        check(w.query() == beforeInvalid, "all invalid commands leave facts and revision intact");
        w.uiCommands().invokeDirectly(8, false);
        for (int command : {146, 147, 148})
            w.uiCommands().invokeDirectly(command, false);
        w.setSize(1120, 700);
        pump();
        auto* edit = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(edit && edit->columnCount() == 4 && edit->coordinates().width >= 160,
              "four native Edit columns preserve timeline space at minimum window size");
        button = dynamic_cast<juce::TextButton*>(find(w, "edit.comment:" + text(id)));
        check(button && button->getRight() <= button->getParentComponent()->getWidth(),
              "Comments column stays inside track row");
        w.uiCommands().invokeDirectly(9, false);
        pump();
        mix = dynamic_cast<juce::TextButton*>(find(w, "mix.comment:" + text(id)));
        auto* fader = find(w, "track.gain:" + text(id));
        auto* law = find(w, "track.pan_law:" + text(id));
        check(mix && fader && law && !fader->getBounds().intersects(mix->getBounds()) &&
                  !fader->getBounds().intersects(law->getBounds()) && fader->getHeight() >= 32,
              "compact Mix preserves usable fader pan-law and Comments without overlap");
        w.uiCommands().invokeDirectly(8, false);
        pump();
        auto keys = w.uiCommands().getKeyMappings();
        keys->clearAllKeyPresses(153);
        const juce::KeyPress custom('j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 'j');
        keys->addKeyPress(153, custom);
        pump();
        c.save(dir.getChildFile("comments.tracktionedit"));
        const auto saved = w.query(), savedView = w.queryView();
        Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("reopened")));
        reopened.setVisible(true);
        reopened.setSize(1120, 700);
        reopened.openSession(dir.getChildFile("comments.tracktionedit"));
        pump();
        check(reopened.query()["tracks"] == saved["tracks"],
              "real new Workspace reopen preserves all track text and stable IDs");
        check(reopened.queryView()["edit_views"] == savedView["edit_views"] && reopened.queryView()["ui_schema"] == 11,
              "column visibility survives native save and reopen");
        check(reopened.uiCommands().getKeyMappings()->containsMapping(153, custom),
              "custom Comments shortcut survives reopen");
        check(reopened.keyPressed(custom), "restored custom shortcut opens selected track Comments");
        pump();
        check(editor(reopened).getText().toStdString() == value, "reopened shortcut editor reads actual saved text");
        reopened.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        juce::ValueTree meta("NATIVEDAW"), ui("UI");
        auto old = savedView;
        for (const auto* key : {"rulers", "main_time_scale", "timecode_fps", "track_heights", "zoom_presets",
                                "track_views", "zoom_state", "waveform_zoom", "midi_zoom"})
            old.erase(key);
        old["ui_schema"] = 4;
        old["edit_views"].erase("comments");
        ui.setProperty("json", juce::String(old.dump()), nullptr);
        meta.addChild(ui, -1, nullptr);
        auto migrated = readUiState(meta);
        check(migrated["ui_schema"] == 11 && migrated["edit_views"]["comments"] == false &&
                  migrated["keymap_xml"] == old["keymap_xml"] &&
                  migrated["selection_tracks"] == old["selection_tracks"],
              "complete schema4 migrates preserving selection layout and user keys");
        old["edit_views"]["extra"] = false;
        ui.setProperty("json", juce::String(old.dump()), nullptr);
        rejects([&] { readUiState(meta); }, "unknown legacy column cannot silently migrate");
        old["edit_views"].erase("extra");
        old["edit_views"]["sends"] = 1;
        ui.setProperty("json", juce::String(old.dump()), nullptr);
        rejects([&] { readUiState(meta); }, "malformed schema4 column rejected");
        check(Commands::mediaHash(source) == mediaHash && track(reopened, id)["clips"] == track(w, id)["clips"],
              "Comments edits and reopen preserve actual media hash clips and routing");
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"ui_schema", 11},
                    {"scope", "native controls with actual Edit and Undo/save/reopen; closed audio device; desktop "
                              "acceptance unexecuted"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("Comments report write failed");
        }
        std::cout << report.dump(2) << '\n';
        dir.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        dir.deleteRecursively();
        return 1;
    }
}
