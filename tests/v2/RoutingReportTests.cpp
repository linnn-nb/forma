// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include "ui/WorkspaceWindow.h"
#include "ProjectDocument.h"
#include "HistoryRuntime.h"
#include "NativeEditXml.h"
#include <juce_cryptography/juce_cryptography.h>
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
    static juce::ValueTree state(Commands& c)
    {
        return c.edit->state.createCopy();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
double maximumError = 0;
Json renders = Json::array(), refusals = Json::array();
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void wait(F f, const char* why)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + 2000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    check(f(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File folder;
    explicit Storage(juce::File f) : te::PropertyStorage("Forma routing report tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }
};
Json op(const char* id, Json args)
{
    return {{"command", id}, {"args", std::move(args)}};
}
Json run(Commands& c, Json operations)
{
    const auto result = c.commit(c.makePlan("human", std::move(operations)));
    check(result.at("state") == "committed", "real common L1 transaction commits");
    return result;
}
Json run(Commands& c, const char* id, Json args)
{
    return run(c, Json::array({op(id, std::move(args))}));
}
juce::String xml(juce::ValueTree t)
{
    return preciseEditXml(t)->toString();
}
juce::String hash(juce::String s)
{
    return juce::SHA256(s.toRawUTF8(), size_t(s.getNumBytesAsUTF8())).toHexString();
}
juce::String editing(Commands& c)
{
    auto t = AudioDeviceTestAccess::state(c);
    history_runtime::project(t);
    auto m = t.getChildWithName("NATIVEDAW");
    m.removeChild(m.getChildWithName("UI"), nullptr);
    return xml(t);
}
juce::ValueTree legacyCurrent(juce::ValueTree t)
{
    auto m = t.getChildWithName("NATIVEDAW");
    m.removeChild(m.getChildWithName("PERSISTENT_HISTORY"), nullptr);
    m.removeChild(m.getChildWithName("UI"), nullptr);
    m.removeProperty("revision", nullptr);
    for (int i = m.getNumChildren(); --i >= 0;)
        if (m.getChild(i).hasType("REQUEST_AUDIT"))
            m.removeChild(i, nullptr);
    return t;
}
juce::Component* find(juce::Component& c, const char* id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* r = find(*child, id))
            return r;
    return nullptr;
}
void chooseGroup(ndaw::desktop::Workspace& w)
{
    auto* combo = dynamic_cast<juce::ComboBox*>(find(w, "project.compatibility.target"));
    check(combo, "native object selector exists");
    int index = -1;
    for (int i = 0; i < combo->getNumItems(); ++i)
        if (combo->getItemText(i).contains("Vocal pair"))
            index = i;
    check(index >= 0, "actual group is enumerated in native report selector");
    combo->setSelectedItemIndex(index, juce::sendNotificationSync);
}
void key(ndaw::desktop::Workspace& w, int id)
{
    auto* keys = w.uiCommands().getKeyMappings();
    auto binding = keys->getKeyPressesAssignedToCommand(id);
    check(!binding.isEmpty() && keys->findCommandForKeyPress(binding[0]) == id,
          "registered shortcut resolves to intended command ID");
    check(keys->keyPressed(binding[0], &w), "actual registered shortcut dispatches");
}
juce::File source(juce::File root)
{
    auto f = root.getChildFile("Source.wav");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = f.createOutputStream();
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "real source writer opens");
    juce::AudioBuffer<float> b(2, 48000);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 48000; ++i)
            b.setSample(ch, i,
                        .16f * float(std::sin(juce::MathConstants<double>::twoPi * (ch ? 660 : 440) * i / 48000.)));
    check(writer->writeFromAudioSampleBuffer(b, 0, 48000), "real PCM24 source frames write");
    return f;
}
juce::AudioBuffer<float> render(Commands& c, juce::File root, const char* why)
{
    auto f = root.getChildFile("Mix-" + juce::String(++serial) + ".wav");
    auto r = c.render(f, 0, 72000);
    check(r.at("audio_verified") == true && r.at("frames") == 72000 && r.at("render_ms").get<double>() < 10000,
          "actual full-range render meets fixed frames and ten-second budget");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->bitsPerSample == 24 &&
              reader->lengthInSamples == 72000,
          "independent WAV reader verifies actual format and full frames");
    juce::AudioBuffer<float> b(2, 72000);
    check(reader->read(&b, 0, 72000, 0, true, true), "all output PCM decodes");
    renders.push_back({{"case", why}, {"receipt", r}});
    return b;
}
void compare(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, double gain)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 72000; ++i)
            error = std::max(error, std::abs(double(a.getSample(ch, i)) - gain * b.getSample(ch, i)));
    maximumError = std::max(maximumError, error);
    check(error <= 2e-6, "complete routing PCM including both edges matches fixed independent gain reference");
}
void refuse(Commands& c, juce::File file, const std::string& expected)
{
    auto before = xml(AudioDeviceTestAccess::state(c));
    auto token = c.sessionToken();
    auto q = c.query();
    std::string error;
    try
    {
        c.open(file);
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }
    check(error.find(expected) != std::string::npos, "real Open rejects intended unsupported group schema");
    check(xml(AudioDeviceTestAccess::state(c)) == before && c.sessionToken() == token &&
              c.query().at("revision") == q.at("revision"),
          "failed Open preserves exact current Edit, session and revision");
    refusals.push_back({{"path", file.getFullPathName().toStdString()}, {"error", error}});
}
// Owned future-producer fixtures, created from scratch with authentic original
// hashes. They do not alter/reseal a damaged modern document to claim success.
juce::File futureHistory(juce::ValueTree base, juce::File root, int archiveSchema, int groupSchema, Json group)
{
    base = legacyCurrent(base.createCopy());
    auto meta = base.getChildWithName("NATIVEDAW");
    meta.removeChild(meta.getChildWithName("MIX_GROUPS"), nullptr);
    auto future = base.createCopy();
    auto fm = future.getChildWithName("NATIVEDAW");
    const auto plan = juce::Uuid().toString();
    juce::ValueTree tx("TRANSACTION");
    tx.setProperty("plan_id", plan, nullptr);
    tx.setProperty("actor", "human", nullptr);
    tx.setProperty("reversible", true, nullptr);
    fm.addChild(tx, -1, nullptr);
    juce::ValueTree groups("MIX_GROUPS"), record("GROUP");
    groups.setProperty("schema", groupSchema, nullptr);
    for (const char* k : {"id", "name"})
        record.setProperty(k, juce::String(group.at(k).get<std::string>()), nullptr);
    for (const char* k : {"enabled", "mute", "solo", "edit"})
        record.setProperty(k, group.at(k).get<bool>(), nullptr);
    record.setProperty("json", juce::String(group.dump()), nullptr);
    groups.addChild(record, -1, nullptr);
    fm.addChild(groups, -1, nullptr);
    juce::ValueTree archive("PERSISTENT_HISTORY");
    archive.setProperty("schema", archiveSchema, nullptr);
    archive.setProperty("cursor", 0, nullptr);
    archive.setProperty("current_hash", hash(xml(base)), nullptr);
    auto add = [&](juce::ValueTree value, const char* type, bool identified)
    {
        juce::ValueTree entry(type);
        const auto data = xml(value);
        entry.setProperty("xml", data, nullptr);
        entry.setProperty("sha256", hash(data), nullptr);
        if (identified)
            entry.setProperty("plan_id", plan, nullptr);
        archive.addChild(entry, -1, nullptr);
    };
    if (archiveSchema == 1)
    {
        add(base, "STATE", false);
        add(future, "STATE", true);
    }
    else
    {
        add(history_delta::difference(base, future), "STEP", true);
        archive.addChild(juce::ValueTree("BLOCKS"), -1, nullptr);
    }
    meta.addChild(archive, -1, nullptr);
    auto path = root.getChildFile("Group-" + juce::String(groupSchema) + "-History-" + juce::String(archiveSchema) +
                                  ".tracktionedit");
    auto doc = project_document::write(base, {});
    check(doc->writeTo(path), "owned original-checksummed legacy history fixture writes");
    return path;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P2-Routing-Report-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << Json{{"result", "running"}, {"fixture", root.getFullPathName().toStdString()}}.dump(2);
        }
        ndaw::desktop::WorkspaceWindow window(
            std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        auto& w = window.editor();
        window.setVisible(true);
        wait([&] { return window.getPeer() && w.isShowing(); },
             "real native peer exists without claiming OS keyboard focus");
        auto& c = AudioDeviceTestAccess::commands(w);
        const auto media = source(root);
        const auto mediaHash = Commands::mediaHash(media);
        run(c, Json::array(
                   {op("track.create", {{"name", "Voice A"}, {"ref", "$a"}}),
                    op("track.create", {{"name", "Voice B"}, {"ref", "$b"}}),
                    op("track.create", {{"name", "Pre Aux"}, {"type", "aux"}, {"ref", "$pre"}}),
                    op("track.create", {{"name", "Post Aux"}, {"type", "aux"}, {"ref", "$post"}}),
                    op("clip.import",
                       {{"track", "$a"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}}),
                    op("clip.import",
                       {{"track", "$b"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}}),
                    op("track.output", {{"track", "$a"}, {"target", "none"}}),
                    op("track.output", {{"track", "$b"}, {"target", "none"}}),
                    op("track.gain", {{"track", "$a"}, {"db", -6}}), op("track.gain", {{"track", "$pre"}, {"db", 0}}),
                    op("track.gain", {{"track", "$post"}, {"db", 0}}),
                    op("send.create", {{"track", "$a"}, {"target", "$pre"}, {"db", -6}, {"position", "pre"}}),
                    op("send.create", {{"track", "$a"}, {"target", "$post"}, {"db", -12}, {"position", "post"}})}));
        const auto tracks = c.query().at("tracks");
        const std::string a = tracks[0].at("id"), b = tracks[1].at("id"), pre = tracks[2].at("id");
        Json group = {{"id", "vocal-pair"}, {"name", "Vocal pair"}, {"members", Json::array({a, b})},
                      {"enabled", true},    {"mute", true},         {"solo", true},
                      {"edit", true}};
        const auto definition = run(c, "group.create", group);
        auto stateBefore = xml(AudioDeviceTestAccess::state(c));
        const auto report = c.projectCompatibility();
        check(xml(AudioDeviceTestAccess::state(c)) == stateBefore,
              "read-only routing and group report leaves exact native state untouched");
        check(report.at("groups")[0].at("type") == "edit_mix" && report.at("routes").size() == 4 &&
                  report.at("routes")[0].at("sends").size() == 2,
              "actual group type and both native sends appear in report");
        check(report.at("routes")[0].at("output").at("target") == "none" &&
                  report.at("routes")[0].at("sends")[0].at("position") == "pre" &&
                  report.at("routes")[0].at("sends")[1].at("position") == "post" && report.at("issues").empty(),
              "report preserves real main output and actual pre/post slots");
        Commands reference(false, std::make_unique<Storage>(root.getChildFile("reference")));
        run(reference, Json::array({op("track.create", {{"name", "Independent reference"}, {"ref", "$s"}}),
                                    op("clip.import", {{"track", "$s"},
                                                       {"path", media.getFullPathName().toStdString()},
                                                       {"position_samples", 0}}),
                                    op("track.gain", {{"track", "$s"}, {"db", 0}})}));
        const auto original = render(reference, root, "independent direct source");
        const double gain = std::pow(10., -6. / 20) + std::pow(10., -18. / 20);
        const auto live = render(c, root, "actual Pre plus Post paths");
        compare(live, original, gain);
        run(c, "track.mute", {{"track", a}, {"enabled", true}});
        check(c.query().at("tracks")[1].at("mute") == true, "actual group Mute reaches peer track");
        auto silent = render(c, root, "actual group Mute");
        check(silent.getMagnitude(0, 72000) <= 2e-6, "group Mute silences complete routed PCM");
        c.undo();
        compare(render(c, root, "group Mute Undo"), live, 1.);
        c.undo(definition.at("plan_id"));
        const auto redo = root.getChildFile("Group-Redo-Only.tracktionedit");
        c.save(redo);
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("cold")));
            cold.open(redo);
            check(cold.projectCompatibility().at("groups").empty(),
                  "cold Redo-only current report has no invented active group");
            cold.redo();
            check(cold.projectCompatibility().at("groups") == report.at("groups"),
                  "original cold Redo restores exact group definition");
            compare(render(cold, root, "cold original group Redo"), live, 1.);
            cold.undo();
        }
        c.redo();
        // Stable main-output identity remains distinct from native track ordinals.
        run(c, "track.output", {{"track", b}, {"target", pre}});
        run(c, "track.order", {{"track", pre}, {"index", 0}});
        bool stable = false;
        const auto reordered = c.projectCompatibility();
        for (const auto& route : reordered.at("routes"))
            if (route.at("track") == b)
                stable = route.at("output").at("target") == pre && route.at("stored_output_target") == pre;
        check(stable, "report reads original stable output target after actual track reorder");
        c.undo();
        c.undo();
        const auto base = legacyCurrent(AudioDeviceTestAccess::state(c));
        for (int format : {1, 2, 3})
        {
            auto valid = futureHistory(base, root, format, 2, group);
            Commands control(false,
                             std::make_unique<Storage>(root.getChildFile("history-control-" + juce::String(format))));
            control.open(valid);
            check(control.query().at("mix_groups").empty(),
                  "authentic original history positive control opens without active group");
            control.redo();
            check(control.query().at("mix_groups")[0].at("id") == "vocal-pair",
                  "positive control original Redo restores group schema2");
            control.undo();
            refuse(c, futureHistory(base, root, format, 3, group), "组元数据版本");
        }
        // Explicit historyless legacy imports, never resealed damaged modern archives.
        auto broken = legacyCurrent(AudioDeviceTestAccess::state(c));
        auto trackA = broken.getChildWithProperty("id", juce::String(a));
        broken.removeChild(broken.getChildWithProperty("id", juce::String(b)), nullptr);
        broken.removeChild(broken.getChildWithProperty("id", juce::String(pre)), nullptr);
        trackA.setProperty("ndaw_output_target", juce::String(b), nullptr);
        const auto missing = root.getChildFile("Legacy-Missing-References.tracktionedit");
        check(preciseEditXml(broken)->writeTo(missing), "owned historyless missing-reference import writes");
        {
            Commands damaged(false, std::make_unique<Storage>(root.getChildFile("missing")));
            damaged.open(missing);
            const auto d = damaged.projectCompatibility();
            check(d.at("groups")[0].at("missing_members") == Json::array({b}) && d.at("issues").size() >= 3,
                  "actual missing group, output and send references are reported");
            bool kept = false, unresolved = false;
            for (const auto& route : d.at("routes"))
                if (route.at("track") == a)
                {
                    kept = route.at("stored_output_target") == b && route.at("output").at("target") == "none" &&
                           route.at("stored_target_missing") == true;
                    unresolved = route.at("sends")[0].at("targets").empty();
                }
            check(kept && unresolved,
                  "missing stable ID and native bus remain unchanged instead of retargeting by ordinal");
        }
        c.updateUiState(
            {{"workspace", "mix"}, {"first_row", 3}, {"selection_tracks", Json::array({tracks[3].at("id")})}},
            c.sessionToken());
        AudioDeviceTestAccess::refresh(w);
        const auto audioState = editing(c);
        const auto revision = c.query().at("revision");
        key(w, 653);
        wait([&] { return w.projectCompatibilityPanel && w.projectCompatibilityPanel->isVisible(); },
             "native report opens through registered shortcut");
        chooseGroup(w);
        auto* text = dynamic_cast<juce::TextEditor*>(find(w, "project.compatibility.report"));
        check(text && text->getText().contains("PRE -6.0") && text->getText().contains("POST -12.0") &&
                  text->getText().contains("Vocal pair"),
              "native visible report includes actual send units and group name");
        auto image = root.getChildFile("Routing-Report-Native.png");
        juce::Image pixels(juce::Image::ARGB, w.getWidth(), w.getHeight(), true, juce::SoftwareImageType{});
        {
            juce::Graphics g(pixels);
            w.paintEntireComponent(g, true);
        }
        {
            auto stream = image.createOutputStream();
            check(stream && juce::PNGImageFormat().writeImageToStream(pixels, *stream),
                  "actual native component software image writes");
        }
        key(w, 654);
        wait(
            [&]
            {
                return !w.projectCompatibilityPanel->isVisible() &&
                       w.queryView().at("selection_tracks") == group.at("members");
            },
            "actual native locate selects complete group and closes report on view receipt");
        check(w.queryView().at("first_row") == 0 && w.queryView().at("workspace") == "edit" &&
                  editing(c) == audioState && c.query().at("revision") == revision,
              "navigation reveals offscreen group through saved UI without changing editing state or Undo revision");
        key(w, 653);
        wait([&] { return w.projectCompatibilityPanel->isVisible(); }, "native report reopens");
        chooseGroup(w);
        run(c, "track.rename", {{"track", a}, {"name", "Voice A revised"}});
        AudioDeviceTestAccess::refresh(w);
        check(!w.projectCompatibilityPanel->canLocate(), "new L1 edit invalidates stale report navigation");
        auto* refresh = dynamic_cast<juce::Button*>(find(w, "project.compatibility.refresh"));
        check(refresh, "actual report refresh button exists");
        refresh->triggerClick();
        wait([&] { return w.projectCompatibilityPanel->canLocate(); },
             "one asynchronous refresh revalidates current group target");
        key(w, 654);
        wait([&] { return !w.projectCompatibilityPanel->isVisible(); },
             "refreshed native locate obtains actual view receipt");
        c.undo();
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom(juce::KeyPress::F19Key,
                                    juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier, 0);
        check(keys->findCommandForKeyPress(custom) == 0, "custom navigation shortcut is unassigned before binding");
        keys->clearAllKeyPresses(654);
        keys->addKeyPress(654, custom);
        wait([&] { return w.queryView().value("keymap_xml", std::string{}).find("F19") != std::string::npos; },
             "custom navigation shortcut enters persisted UI state");
        const auto delivery = root.getChildFile("Routing-Report-Delivered.tracktionedit");
        const auto saved = c.save(delivery);
        check(saved.at("required_capabilities").size() == 1 && juce::parseXML(delivery)->getIntAttribute("schema") == 1,
              "known group and native routes retain supported schema1 without inventing capabilities");
        w.openSession(delivery);
        check(keys->findCommandForKeyPress(custom) == 654, "custom navigation key survives actual saved reopen");
        key(w, 653);
        wait([&] { return w.projectCompatibilityPanel->isVisible(); }, "cold native report opens");
        chooseGroup(w);
        check(keys->keyPressed(custom, &w), "reopened custom key dispatches actual navigation");
        wait([&] { return !w.projectCompatibilityPanel->isVisible(); },
             "reopened custom navigation receives actual view receipt");
        c.undo();
        check(c.query().at("mix_groups").empty(), "report and navigation do not consume original saved group Undo");
        c.redo();
        check(c.query().at("mix_groups")[0].at("id") == "vocal-pair", "original saved group Redo remains executable");
        compare(render(c, root, "reopened original group Redo"), live, 1.);
        check(Commands::mediaHash(media) == mediaHash,
              "all report, navigation, failed Open and rendering preserve original media hash");
        Json result = {{"result", "passed"},
                       {"checks", checks},
                       {"fixture", root.getFullPathName().toStdString()},
                       {"full_pcm_max_error", maximumError},
                       {"report", report},
                       {"renders", renders},
                       {"refusals", refusals},
                       {"native_component_image", image.getFullPathName().toStdString()},
                       {"scope", "actual native peer and command mappings; software component image, not OS keyboard, "
                                 "hardware or listening qualification"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2);
            if (!out)
                throw std::runtime_error("result write failed");
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
