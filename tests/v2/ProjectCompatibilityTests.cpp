// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include "ui/WorkspaceWindow.h"
#include "ProjectDocument.h"
#include "NativeEditXml.h"
#include "HistoryDelta.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Commands& commands(ndaw::desktop::Workspace& w)
    {
        return w.commands;
    }
    static void refresh(ndaw::desktop::Workspace& w)
    {
        w.refresh();
    }
    static juce::String xml(Commands& c)
    {
        return preciseEditXml(c.edit->state)->toString();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
Json refusals = Json::array();
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void wait(F f, const char* why, double ms = 2000)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + ms;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    check(f(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File dir;
    explicit Storage(juce::File f) : te::PropertyStorage("Forma compatibility tests"), dir(f) {}
    juce::File getAppPrefsFolder() override
    {
        dir.createDirectory();
        return dir;
    }
};
Json run(Commands& c, const char* command, Json args)
{
    auto r = c.commit(c.makePlan("human", Json::array({{{"command", command}, {"args", args}}})));
    check(r.at("state") == "committed", "real common L1 edit commits");
    return r;
}
juce::Component* find(juce::Component& c, const char* id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* f = find(*child, id))
            return f;
    return nullptr;
}
void refuse(Commands& c, juce::XmlElement& xml, juce::File path, const std::string& reason)
{
    check(xml.writeTo(path), "owned incompatible fixture writes");
    const auto state = AudioDeviceTestAccess::xml(c), token = juce::String(c.sessionToken());
    const auto version = c.query().at("revision");
    std::string error;
    try
    {
        c.open(path);
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }
    check(error.find(reason) != std::string::npos, "real Open refuses intended unsupported or undeclared capability");
    check(AudioDeviceTestAccess::xml(c) == state && c.sessionToken() == token.toStdString() &&
              c.query().at("revision") == version,
          "failed Open preserves exact native Edit, history, session and revision");
    refusals.push_back({{"path", path.getFullPathName().toStdString()}, {"reason", error}});
}
void descriptors()
{
    juce::ValueTree before("EDIT"), track("TRACK"), plugin("PLUGIN"), opaque("PRIVATE"), fake("NDAW_BOUNCE");
    track.setProperty("id", "1001", nullptr);
    plugin.setProperty("id", "1002", nullptr);
    fake.setProperty("schema", 999, nullptr);
    fake.setProperty("ndaw_freeze_file", "/opaque/vendor-data", nullptr);
    opaque.addChild(fake, -1, nullptr);
    plugin.addChild(opaque, -1, nullptr);
    track.addChild(plugin, -1, nullptr);
    before.addChild(track, -1, nullptr);
    project_document::Requirements requirements;
    project_document::collect(before, requirements);
    check(requirements.empty(), "opaque plugin child tags never become project capabilities");
    auto after = before.createCopy();
    after.getChild(0).getChild(0).setProperty("ndaw_print_baked", true, nullptr);
    project_document::collect(history_delta::difference(before, after), requirements);
    check(requirements.size() == 1 && requirements.at("audio.print.bounce-playlist") == 1,
          "actual host plugin-bypass property delta declares Bounce even without its track backup");
    requirements.clear();
    after = before.createCopy();
    after.getChild(0).getChild(0).getChild(0).getChild(0).setProperty("ndaw_print_baked", true, nullptr);
    project_document::collect(history_delta::difference(before, after), requirements);
    check(requirements.empty(), "opaque plugin property history cannot spoof host print or freeze capabilities");
    after = before.createCopy();
    after.getChild(0).setProperty("ndaw_freeze_file", "/owned/cache.wav", nullptr);
    project_document::collect(history_delta::difference(before, after), requirements);
    check(requirements.at("audio.print.freeze-cache") == 1,
          "freeze path-only original history declares custom cache semantics");
    requirements.clear();
    juce::ValueTree lists("NDAW_PLAYLISTS"), inactive("PLAYLIST");
    inactive.setProperty("role", "bounce", nullptr);
    lists.addChild(inactive, -1, nullptr);
    project_document::collect(lists, requirements);
    check(requirements.at("audio.print.bounce-playlist") == 1,
          "inactive printed Playlist alone retains its editing capability");
}
juce::File source(juce::File root)
{
    const auto file = root.getChildFile("Original.wav");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "real controlled PCM24 source opens");
    juce::AudioBuffer<float> pcm(2, 48000);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 48000; ++i)
            pcm.setSample(ch, i,
                          .12f * float(std::sin(juce::MathConstants<double>::twoPi * (ch ? 660 : 440) * i / 48000.)));
    check(writer->writeFromAudioSampleBuffer(pcm, 0, 48000), "actual stereo source samples write");
    return file;
}
Json print(Commands& c, const std::string& track, juce::File root, const char* action)
{
    c.prepareTrackPrint({{"track", track},
                         {"action", action},
                         {"end_samples", 72000},
                         {"directory", root.getFullPathName().toStdString()},
                         {"session_token", c.sessionToken()},
                         {"base_revision", c.query().at("revision")}});
    wait([&] { return !c.trackPrintStatus().at("busy").get<bool>(); }, "real print completes within fixed 10s", 10000);
    const auto receipt = c.trackPrintStatus().at("receipt");
    check(receipt.at("state") == "ready" && receipt.at("audio_verified") == true,
          "real print returns verified artifact before edit");
    run(c, std::string(action) == "freeze" ? "track.freeze" : "track.bounce.in_place",
        {{"track", track}, {"render_id", receipt.at("render_id")}});
    return receipt;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P2-Compatibility-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << Json{{"result", "running"}, {"fixture", root.getFullPathName().toStdString()}}.dump(2);
            if (!out)
                throw std::runtime_error("initial report write failed");
        }
        descriptors();
        ndaw::desktop::WorkspaceWindow window(
            std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        auto& w = window.editor();
        window.setVisible(true);
        wait([&] { return window.getPeer() && w.isShowing(); },
             "real native peer exists without claiming OS keyboard focus");
        auto& c = AudioDeviceTestAccess::commands(w);
        const auto media = source(root);
        const auto hash = juce::String(Commands::mediaHash(media));
        run(c, "track.create", {{"name", "Compatibility print"}, {"ref", "$print"}});
        const std::string track = c.query().at("tracks")[0].at("id");
        run(c, "clip.import",
            {{"track", track}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
        run(c, "plugin.insert", {{"track", track}, {"type", "compressor"}});
        const std::string plugin = c.query().at("tracks")[0].at("plugins")[0].at("id");
        run(c, "track.create", {{"name", "Native trigger"}, {"ref", "$trigger"}});
        const std::string trigger = c.query().at("tracks")[1].at("id");
        run(c, "plugin.sidechain.set", {{"plugin", plugin}, {"source", trigger}});
        const auto native = root.getChildFile("Native-Sidechain.tracktionedit");
        const auto nativeSaved = c.save(native);
        check(!nativeSaved.at("required_capabilities").contains("audio.print.freeze-cache") &&
                  c.projectCompatibility().at("sidechains").size() == 1,
              "native sidechain reports real wiring without inventing a print requirement");
        const auto frozen = print(c, track, root, "freeze");
        const auto frozenFile = root.getChildFile("Frozen.tracktionedit");
        const auto saved = c.save(frozenFile);
        check(saved.at("required_capabilities").at("audio.print.freeze-cache") == 1,
              "saved real Freeze declares required custom reader capability");
        auto xml = juce::parseXML(frozenFile);
        auto missing = std::make_unique<juce::XmlElement>(*xml);
        auto* features = missing->getChildByName("REQUIRES");
        for (auto* f = features->getFirstChildElement(); f; f = f->getNextElement())
            if (f->getStringAttribute("id") == "audio.print.freeze-cache")
            {
                features->removeChildElement(f, true);
                break;
            }
        refuse(c, *missing, root.getChildFile("Missing-Print-Requirement.tracktionedit"), "声明遗漏");
        auto legacy = std::make_unique<juce::XmlElement>(*missing);
        legacy->setAttribute("schema", 1);
        const auto legacyFile = root.getChildFile("Legacy-Schema1-Freeze.tracktionedit");
        check(legacy->writeTo(legacyFile),
              "owned old schema1 print-omission fixture keeps original payload and history hashes");
        const auto legacyHash = Commands::mediaHash(legacyFile);
        {
            Commands old(false, std::make_unique<Storage>(root.getChildFile("legacy-prefs")));
            old.open(legacyFile);
            check(old.projectCompatibility().at("legacy_print_audit") == true &&
                      old.projectCompatibility().at("prints").size() == 1,
                  "authenticated old schema1 Freeze opens with visible finite capability audit");
            old.undo();
            old.redo();
            const auto upgraded = root.getChildFile("Migrated-Schema1-Freeze.tracktionedit");
            check(old.save(upgraded).at("required_capabilities").at("audio.print.freeze-cache") == 1 &&
                      juce::parseXML(upgraded)->getIntAttribute("schema") == 2,
                  "saving migrated old Freeze declares strict schema2 without rerendering audio");
            check(Commands::mediaHash(legacyFile) == legacyHash,
                  "legacy reader migration leaves original file bytes untouched");
            old.saveAs(root.getChildFile("Migrated-Bound-Schema2.tracktionedit"));
            check(old.projectCompatibility().at("legacy_print_audit") == false &&
                      Commands::mediaHash(legacyFile) == legacyHash,
                  "successful Save As binds strict schema2 report while preserving original old file");
        }
        auto future = std::make_unique<juce::XmlElement>(*xml);
        for (auto* f = future->getChildByName("REQUIRES")->getFirstChildElement(); f; f = f->getNextElement())
            if (f->getStringAttribute("id") == "audio.print.freeze-cache")
                f->setAttribute("version", 999);
        refuse(c, *future, root.getChildFile("Future-Print-Requirement.tracktionedit"), "freeze-cache v999");
        c.undo();
        const auto redoFile = root.getChildFile("Freeze-Redo-Only.tracktionedit");
        check(c.save(redoFile).at("required_capabilities").at("audio.print.freeze-cache") == 1,
              "saved original Freeze Redo alone still protects the project");
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("cold")));
            cold.open(redoFile);
            auto r = cold.projectCompatibility();
            bool historyOnly = false;
            for (const auto& f : r.at("capabilities"))
                if (f.at("id") == "audio.print.freeze-cache")
                    historyOnly = f.at("history_only");
            check(historyOnly && r.at("prints").empty(),
                  "cold report distinguishes history-only Freeze from active printed audio");
            cold.redo();
            check(cold.projectCompatibility().at("prints")[0].at("state").at("frozen") == true,
                  "actual original cold Redo restores Freeze and current report");
            cold.undo();
        }
        c.redo();
        run(c, "track.unfreeze", {{"track", track}});
        const auto bounced = print(c, track, root, "bounce");
        const auto bounceFile = root.getChildFile("Bounce.tracktionedit");
        check(c.save(bounceFile).at("required_capabilities").at("audio.print.bounce-playlist") == 1,
              "real original Playlist and printed bypass declare Bounce reader capability");
        {
            Commands isolated(false, std::make_unique<Storage>(root.getChildFile("bounce-only-prefs")));
            isolated.open(native);
            print(isolated, track, root, "bounce");
            const auto only = isolated.save(root.getChildFile("Bounce-Only.tracktionedit"));
            check(only.at("required_capabilities").contains("audio.print.bounce-playlist") &&
                      !only.at("required_capabilities").contains("audio.print.freeze-cache"),
                  "separate actual Bounce fixture requires only Bounce, not an unrelated earlier Freeze");
        }
        run(c, "track.bounce.restore", {{"track", track}});
        const auto inactiveFile = root.getChildFile("Inactive-Print.tracktionedit");
        check(c.save(inactiveFile).at("required_capabilities").at("audio.print.bounce-playlist") == 1,
              "restored original with inactive printed Playlist retains capability");
        w.openSession(bounceFile);
        const auto before = AudioDeviceTestAccess::xml(c);
        const auto facts = c.query();
        const auto report = c.projectCompatibility();
        check(report.at("reader_supported") == true && report.at("prints").size() == 1 &&
                  report.at("sidechains").size() == 1,
              "L1 report derives current real print, native sidechain and full capability closure");
        auto* keys = w.uiCommands().getKeyMappings();
        const auto bindings = keys->getKeyPressesAssignedToCommand(653);
        check(!bindings.isEmpty() && keys->findCommandForKeyPress(bindings[0]) == 653,
              "default registered shortcut resolves to intended compatibility command");
        check(keys->keyPressed(bindings[0], &w), "actual command mapping dispatches to native report");
        wait([&] { return w.projectCompatibilityPanel && w.projectCompatibilityPanel->isVisible(); },
             "registered shortcut opens real native report");
        auto* body = dynamic_cast<juce::TextEditor*>(find(w, "project.compatibility.report"));
        check(body && body->isReadOnly() && body->getText().contains("audio.print.bounce-playlist") &&
                  body->getText().contains(juce::String::fromUTF8("Tracktion原生")),
              "visible selectable native report contains actual print requirements and native sidechain scope");
        w.uiCommands().invokeDirectly(6, false);
        check(AudioDeviceTestAccess::xml(c) == before && c.query().at("revision") == facts.at("revision"),
              "viewing report and covered-editor Undo cannot mutate Edit or history");
        const auto screenshot = root.getChildFile("Project-Compatibility-Native.png");
        juce::Image image(juce::Image::ARGB, w.getWidth(), w.getHeight(), true, juce::SoftwareImageType{});
        {
            juce::Graphics g(image);
            w.paintEntireComponent(g, true);
        }
        {
            auto stream = screenshot.createOutputStream();
            check(stream && juce::PNGImageFormat().writeImageToStream(image, *stream),
                  "actual software-rendered native component image writes");
        }
        run(c, "track.gain", {{"track", track}, {"db", -6}});
        AudioDeviceTestAccess::refresh(w);
        auto* label = dynamic_cast<juce::Label*>(find(w, "project.compatibility.version"));
        check(label && label->getText().contains(juce::String::fromUTF8("请刷新")),
              "concurrent L1 edit marks visible report stale");
        auto* refresh = dynamic_cast<juce::Button*>(find(w, "project.compatibility.refresh"));
        check(refresh, "native report has real refresh control");
        refresh->triggerClick();
        wait([&] { return label && !label->getText().contains(juce::String::fromUTF8("请刷新")); },
             "one asynchronous refresh receives new actual L1 version");
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !w.projectCompatibilityPanel->isVisible(),
              "Escape returns to editor");
        const juce::KeyPress custom(juce::KeyPress::F18Key,
                                    juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier, 0);
        check(keys->findCommandForKeyPress(custom) == 0, "custom compatibility key is free before binding");
        keys->clearAllKeyPresses(653);
        keys->addKeyPress(653, custom);
        wait([&] { return w.queryView().value("keymap_xml", std::string{}).find("F18") != std::string::npos; },
             "custom report key persists through existing UI path");
        const auto final = root.getChildFile("Compatibility-Delivered.tracktionedit");
        c.save(final);
        w.openSession(final);
        check(keys->findCommandForKeyPress(custom) == 653 && keys->keyPressed(custom, &w),
              "custom report shortcut survives saved reopen and dispatches actual command");
        wait([&] { return w.projectCompatibilityPanel->isVisible(); }, "reopened customized shortcut opens report");
        w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        c.undo();
        check(c.query().at("tracks")[0].at("gain_db").get<double>() > -6,
              "read-only report and custom binding leave original reopened Undo executable");
        c.redo();
        check(Commands::mediaHash(media) == hash.toStdString() &&
                  Commands::mediaHash(juce::File(juce::String(frozen.at("path").get<std::string>()))) ==
                      frozen.at("media_hash").get<std::string>() &&
                  Commands::mediaHash(juce::File(juce::String(bounced.at("path").get<std::string>()))) ==
                      bounced.at("media_hash").get<std::string>(),
              "source and both previously printed files remain byte-identical without rerender");
        Json result = {{"result", "passed"},
                       {"checks", checks},
                       {"fixture", root.getFullPathName().toStdString()},
                       {"report", report},
                       {"refusals", refusals},
                       {"prints", Json::array({frozen, bounced})},
                       {"native_component_image", screenshot.getFullPathName().toStdString()},
                       {"scope", "actual native peer and command dispatch; software component image, not OS "
                                 "screenshot, physical keyboard or listening qualification"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        Json r = {{"result", "failed"},
                  {"checks", checks},
                  {"fixture", root.getFullPathName().toStdString()},
                  {"error", e.what()}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << r.dump(2);
        }
        std::cerr << r.dump(2) << std::endl;
        return 1;
    }
}
