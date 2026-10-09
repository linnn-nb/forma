#include "ui/WorkspaceWindow.h"
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
void check(bool result, const char* description)
{
    if (!result)
        throw std::runtime_error(description);
    ++checks;
    std::cout << "PASS " << description << std::endl;
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
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File p) : PropertyStorage("Forma Memory roll tests"), folder(p) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
Json run(Commands& c, const char* cmd, Json args)
{
    const auto receipt = c.commit(c.makePlan("human", Json::array({operation(cmd, args)})));
    pump();
    return receipt;
}
Json roll(int64_t pre, int64_t post, bool enabledPre = false, bool enabledPost = false)
{
    return {{"pre_samples", pre}, {"post_samples", post}, {"pre_enabled", enabledPre}, {"post_enabled", enabledPost}};
}
Json currentRoll(Commands& c)
{
    return c.query()["transport_settings"]["roll"];
}
Json location(Commands& c, const std::string& id)
{
    const auto markers = c.query()["markers"];
    for (const auto& m : markers)
        if (m["id"] == id)
            return m;
    throw std::runtime_error("missing test location");
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* f = find(*child, id))
            return f;
    return nullptr;
}
void click(Workspace& w, const char* id)
{
    auto* button = dynamic_cast<juce::Button*>(find(w, id));
    check(button && button->isEnabled(), "native Memory Location action is present and enabled");
    button->triggerClick();
    pump();
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& file)
{
    const auto receipt = c.render(file, 0, 180000);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    check(reader && reader->sampleRate == 48000 && reader->lengthInSamples == 180000 && reader->numChannels == 2 &&
              receipt["frames"] == 180000,
          "real Tracktion render independently decoded as stereo 48k WAV");
    juce::AudioBuffer<float> pcm(2, 180000);
    check(reader->read(&pcm, 0, 180000, 0, true, true), "actual rendered PCM read successfully");
    return pcm;
}
juce::XmlElement* findRoll(juce::XmlElement& xml)
{
    if (xml.hasTagName("NDAW_LOCATION_ROLL"))
        return &xml;
    for (auto* child = xml.getFirstChildElement(); child; child = child->getNextElement())
        if (auto* result = findRoll(*child))
            return result;
    return nullptr;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                      .getChildFile("forma-memory-roll-" + juce::Uuid().toString());
    if (argc > 2)
        folder = juce::File(juce::String::fromUTF8(argv[2])).getChildFile(juce::Uuid().toString());
    folder.createDirectory();
    try
    {
        const auto source = folder.getChildFile("MemoryRollDiagnostic.wav");
        check(!source.exists(), "owned diagnostic never overwrites user source files");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = source.createOutputStream();
        auto writer = format.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        juce::AudioBuffer<float> samples(2, 180000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < samples.getNumSamples(); ++i)
                samples.setSample(
                    ch, i, float(.09 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 659 : 431) * i / 48000.)));
        check(writer && writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples()),
              "real low-amplitude stereo PCM written");
        writer.reset();
        const auto hash = Commands::mediaHash(source);
        WorkspaceWindow window(
            std::make_unique<Workspace>(false, std::make_unique<Storage>(folder.getChildFile("prefs"))));
        auto& w = window.editor();
        auto& c = AudioDeviceTestAccess::owner(w);
        c.commit(
            c.makePlan("human", Json::array({operation("track.create", {{"name", "Memory roll audio"}, {"ref", "$t"}}),
                                             operation("clip.import", {{"track", "$t"},
                                                                       {"path", source.getFullPathName().toStdString()},
                                                                       {"position_samples", 0},
                                                                       {"ref", "$c"}})})));
        window.showReady();
        pump();
        const auto original = render(c, folder.getChildFile("before.wav"));
        run(c, "marker.create", {{"name", "Verse"}, {"position_samples", 120000}});
        const auto marker = c.query()["markers"][0]["id"].get<std::string>();
        check(location(c, marker)["roll_times"].is_null(), "legacy/default markers do not invent stored roll times");
        const auto stored = roll(24001, 12003, true, false);
        run(c, "transport.roll.set", stored);
        auto capture = c.makePlan("human", Json::array({operation("location.roll.capture", {{"marker", marker}})}));
        check(c.preview(capture)["marker_changes"][0]["after"] ==
                  Json({{"pre_samples", 24001}, {"post_samples", 12003}}),
              "capture preview exposes exact durations without enable flags");
        const auto revision = c.query()["revision"].get<uint64_t>();
        auto receipt = c.commit(capture);
        check(c.query()["revision"] == revision + 1 && location(c, marker)["roll_times"]["pre_samples"] == 24001,
              "capture is one real native transaction and revision");
        c.undo(receipt["plan_id"]);
        check(location(c, marker)["roll_times"].is_null() && currentRoll(c) == stored,
              "Undo capture removes only stored times and preserves current settings");
        c.redo();
        check(location(c, marker)["roll_times"]["post_samples"] == 12003,
              "Redo retains exact non-round-sample duration");
        run(c, "session.range.set", {{"start_samples", 48000}, {"end_samples", 96001}});
        run(c, "location.store_selection", {{"name", "Chorus with roll"}});
        std::string selection;
        const auto createdMarkers = c.query()["markers"];
        for (const auto& item : createdMarkers)
            if (item["kind"] == "selection")
                selection = item["id"];
        run(c, "location.roll.capture", {{"marker", selection}});
        const auto other = roll(96000, 48000, false, true);
        run(c, "transport.roll.set", other);
        run(c, "session.range.set", {{"start_samples", 160000}, {"end_samples", 179000}});
        c.seek(168000);
        const auto oldRange = c.query()["time_selection"];
        auto recall = c.makePlan("human", Json::array({operation("location.recall", {{"marker", selection}})}));
        const auto diff = c.preview(recall)["marker_changes"][0];
        check(diff["before"]["position_samples"] == 168000 && diff["before"]["time_selection"] == oldRange &&
                  diff["after"]["position_samples"] == 48000 &&
                  diff["after"]["time_selection"]["end_samples"] == 96001 &&
                  diff["after"]["roll"] == roll(24001, 12003, false, true),
              "recall preview includes cursor, selection and roll while retaining current enable flags");
        auto result = c.commit(recall);
        check(c.query()["position_samples"] == 48000 && c.query()["time_selection"]["end_samples"] == 96001 &&
                  currentRoll(c) == roll(24001, 12003, false, true),
              "real selection recall atomically applies native cursor, range and duration");
        c.undo(result["plan_id"]);
        check(c.query()["position_samples"] == 168000 && c.query()["time_selection"] == oldRange &&
                  currentRoll(c) == other,
              "single Undo restores all three recalled properties");
        c.redo();
        check(c.query()["position_samples"] == 48000 && currentRoll(c) == roll(24001, 12003, false, true),
              "single Redo restores recall and its preserved flags");
        const auto revAfter = c.query()["revision"];
        check(c.commit(recall)["replayed"] == true && c.query()["revision"] == revAfter,
              "retry does not duplicate recall or revisions");
        run(c, "location.recall", {{"marker", marker}});
        check(c.query()["position_samples"] == 120000 && c.query()["time_selection"]["end_samples"] == 96001,
              "marker recall preserves selection instead of replacing it");
        run(c, "location.roll.clear", {{"marker", marker}});
        check(location(c, marker)["roll_times"].is_null(), "clear removes only optional marker-local roll memory");
        c.undo();
        check(location(c, marker)["roll_times"]["pre_samples"] == 24001, "Undo clear restores marker-local data");
        c.redo();
        run(c, "transport.roll.set", other);
        run(c, "location.recall", {{"marker", marker}});
        check(currentRoll(c) == other, "recall of unstored location does not replace current durations with defaults");
        for (const char* command : {"location.roll.capture", "location.roll.clear", "location.recall"})
        {
            const auto registry = Commands::registry();
            check(std::any_of(registry.begin(), registry.end(),
                              [&](const auto& e) { return e["id"] == command && e["tool_visibility"] == "local_gui"; }),
                  "new Memory commands stay out of frozen MCP tool surface");
            rejects([&] { c.makePlan("agent:test", Json::array({operation(command, {{"marker", selection}})})); },
                    "local Memory operation rejects non-human actor");
            rejects([&] { c.makePlan("human", Json::array({operation(command, {{"marker", "missing"}})})); },
                    "missing Memory Location cannot be reported as successful");
            rejects(
                [&]
                {
                    c.makePlan("human", Json::array({operation("session.range.clear", Json::object()),
                                                     operation(command, {{"marker", selection}})}));
                },
                "unsupported compound recall/capture is refused before mutation");
        }
        auto stale = c.makePlan("human", Json::array({operation("location.recall", {{"marker", selection}})}));
        run(c, "marker.rename", {{"marker", marker}, {"name", "Verse renamed"}});
        rejects([&] { c.commit(stale); }, "stale revision recall cannot overwrite subsequent human work");
        run(c, "marker.delete", {{"marker", selection}});
        c.undo();
        check(location(c, selection)["roll_times"]["pre_samples"] == 24001,
              "delete Undo restores same stable ID and stored times");
        run(c, "transport.roll.set", roll(0, 0));
        run(c, "location.roll.capture", {{"marker", marker}});
        check(location(c, marker)["roll_times"] == Json({{"pre_samples", 0}, {"post_samples", 0}}),
              "zero roll durations retain valid explicit memory");
        c.undo();
        const auto saved = folder.getChildFile("MemoryRollDemo.tracktionedit");
        c.save(saved);
        const auto savedMarkers = c.query()["markers"];
        w.openLocalFile(saved);
        pump();
        check(c.query()["markers"] == savedMarkers && currentRoll(c) == roll(0, 0),
              "native Edit save/reopen retains stable locations and optional roll data");
        rejects([&] { c.commit(stale); }, "old-session recall cannot commit after reopening");
        auto root = juce::XmlDocument::parse(saved);
        check(root && findRoll(*root), "stored durations are present in real saved Edit XML");
        for (auto bad : {"-1", "1.5", "NaN", "99999999999999999999", ""})
        {
            auto copy = std::make_unique<juce::XmlElement>(*root);
            findRoll(*copy)->setAttribute("pre_samples", bad);
            auto broken = folder.getChildFile("broken.tracktionedit");
            copy->writeTo(broken);
            rejects([&] { c.open(broken); }, "corrupt saved duration rejected before replacing current Edit");
            check(c.query()["markers"] == savedMarkers, "failed adoption preserves current Memory Locations");
        }
        for (int bad = 0; bad < 3; ++bad)
        {
            auto copy = std::make_unique<juce::XmlElement>(*root);
            auto* node = findRoll(*copy);
            if (bad == 0)
                node->setAttribute("schema", 2);
            if (bad == 1)
                node->setAttribute("pre_enabled", 1);
            if (bad == 2)
                node->createNewChildElement("unexpected");
            auto broken = folder.getChildFile("broken-schema.tracktionedit");
            copy->writeTo(broken);
            rejects([&] { c.open(broken); }, "unknown schema, enable flag or nested saved data rejected");
        }
        auto duplicate = std::make_unique<juce::XmlElement>(*root);
        const auto appendDuplicate = [&](auto&& self, juce::XmlElement& node) -> bool
        {
            if (auto* child = node.getChildByName("NDAW_LOCATION_ROLL"))
            {
                node.addChildElement(new juce::XmlElement(*child));
                return true;
            }
            for (auto* child = node.getFirstChildElement(); child; child = child->getNextElement())
                if (self(self, *child))
                    return true;
            return false;
        };
        check(appendDuplicate(appendDuplicate, *duplicate),
              "duplicate saved-state corruption fixture targets actual marker");
        const auto duplicated = folder.getChildFile("duplicate.tracktionedit");
        duplicate->writeTo(duplicated);
        rejects([&] { c.open(duplicated); }, "duplicate marker-local roll data is not silently selected");
        run(c, "transport.roll.set", roll(std::llround(te::Edit::maximumLength * 48000.), 0));
        run(c, "location.roll.capture", {{"marker", marker}});
        check(location(c, marker)["roll_times"]["pre_samples"] == std::llround(te::Edit::maximumLength * 48000.),
              "maximum allowed duration captures without narrowing");
        c.undo();
        c.undo();
        run(c, "transport.roll.set", other);
        w.showMemoryLocations(selection);
        pump();
        auto* panel = dynamic_cast<MemoryLocationsPanel*>(find(w, "memory.locations.panel"));
        check(panel && panel->canRecall() && panel->canClearRoll(),
              "production panel reflects real selected stored-roll state");
        auto* detail = find(w, "memory.locations.detail");
        auto* captureButton = find(w, "memory.locations.roll.capture");
        check(detail && captureButton && detail->getBottom() <= captureButton->getY(),
              "roll detail and actions do not overlap at actual production size");
        click(w, "memory.locations.go");
        check(currentRoll(c) == roll(24001, 12003, false, true), "production Go To uses atomic L1 recall");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(currentRoll(c) == other, "production Undo reverses actual Go To duration recall");
        w.uiCommands().invokeDirectly(7, false);
        pump();
        click(w, "memory.locations.roll.clear");
        check(location(c, selection)["roll_times"].is_null() && !panel->canClearRoll(),
              "clear button and disabled state follow real receipt and query");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        const juce::KeyPress customCapture(juce::KeyPress::F6Key, juce::ModifierKeys::commandModifier, 0);
        const juce::KeyPress customRecall(juce::KeyPress::F7Key, juce::ModifierKeys::commandModifier, 0);
        auto* keys = w.uiCommands().getKeyMappings();
        keys->clearAllKeyPresses(281);
        keys->addKeyPress(281, customCapture);
        keys->clearAllKeyPresses(275);
        keys->addKeyPress(275, customRecall);
        pump();
        run(c, "transport.roll.set", roll(36001, 18002, true, false));
        w.showMemoryLocations(selection);
        pump();
        check(keys->findCommandForKeyPress(customCapture) == 281,
              "custom capture key has no competing default binding");
        check(panel->handleKey(customCapture), "panel executes real remapped capture key");
        pump();
        check(location(c, selection)["roll_times"]["pre_samples"] == 36001,
              "custom capture updates actual saved times");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(location(c, selection)["roll_times"]["pre_samples"] == 24001,
              "one GUI Undo restores previous saved durations");
        auto staleFacts = c.query();
        run(c, "marker.rename", {{"marker", marker}, {"name", "Current human edit"}});
        const auto expected = c.query();
        panel->bind(staleFacts);
        panel->selectID(selection);
        panel->recallSelected();
        auto* status = dynamic_cast<juce::Label*>(find(w, "memory.locations.status"));
        check(status && status->getText().contains(text("未执行")) && c.query()["revision"] == expected["revision"],
              "stale panel reports failure instead of false success or partial seek");
        w.showMemoryLocations(selection);
        pump();
        check(panel->handleKey(customRecall), "panel executes current custom recall key");
        pump();
        check(currentRoll(c) == roll(24001, 12003, true, false) && c.query()["position_samples"] == 48000,
              "custom recall applies real selection and preserves current switches");
        const auto finalSaved = folder.getChildFile("MemoryRollReady.tracktionedit");
        c.save(finalSaved);
        w.openLocalFile(finalSaved);
        pump();
        check(keys->findCommandForKeyPress(customCapture) == 281 && keys->findCommandForKeyPress(customRecall) == 275,
              "custom Memory keyboard assignments persist across save/reopen");
        w.showMemoryLocations(selection);
        pump();
        check(panel->handleKey(juce::KeyPress(juce::KeyPress::escapeKey)),
              "Escape cancels Memory window through native panel");
        check(!panel->isVisible(), "cancel leaves project intact and returns editing surface");
        run(c, "transport.roll.set", other);
        c.seek(150000);
        pump();
        const auto beforeRulerRange = c.query()["time_selection"];
        auto* area = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(area && area->markerLaneY() >= 0, "real marker ruler is present");
        const auto now = juce::Time::getCurrentTime();
        const juce::Point<float> point(float(area->coordinates().pixelAt(48000)), float(area->markerLaneY() + 10));
        const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(), point,
                                     juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, area, area, now, point, now,
                                     1, false);
        area->mouseDown(event);
        area->mouseUp(event);
        pump();
        check(c.query()["position_samples"] == 48000 && currentRoll(c) == roll(24001, 12003, false, true),
              "actual ruler click recalls stored durations through L1 rather than pre-seeking");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(c.query()["position_samples"] == 150000 && currentRoll(c) == other &&
                  c.query()["time_selection"] == beforeRulerRange,
              "one Undo after ruler click restores cursor, range and settings");
        if (panel->isVisible())
            panel->handleKey(juce::KeyPress(juce::KeyPress::escapeKey));
        const auto rendered = render(c, folder.getChildFile("after.wav"));
        double error = 0, energy = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 180000; ++i)
            {
                error = std::max(error, std::abs(double(original.getSample(ch, i)) - rendered.getSample(ch, i)));
                energy += std::abs(rendered.getSample(ch, i));
            }
        check(energy > 100 && error <= 2e-5, "Memory operations leave all actual rendered PCM unchanged within 2e-5");
        check(Commands::mediaHash(source) == hash, "source media bytes remain unchanged");
        window.setVisible(false);
        Json report{
            {"test", "U-P0-MEMORY-ROLL-01"},
            {"result", "passed"},
            {"checks", checks},
            {"pcm_max_error", error},
            {"pcm_tolerance", 2e-5},
            {"source_sha256", hash},
            {"demo", finalSaved.getFullPathName().toStdString()},
            {"scope", "native Edit, Undo, saved XML, production panel/key map and decoded render; desktop separate"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
        }
        std::cout << "PASS " << checks << " checks\n";
        if (argc <= 2)
            folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
