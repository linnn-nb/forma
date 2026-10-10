#include "Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Json history(Commands& c)
    {
        Json transactions = Json::array();
        for (int i = 0; i < c.metadata.getNumChildren(); ++i)
        {
            auto tx = c.metadata.getChild(i);
            if (!tx.hasType("TRANSACTION"))
                continue;
            transactions.push_back({{"id", tx.getProperty("plan_id").toString().toStdString()},
                                    {"source", tx.getProperty("source").toString().toStdString()},
                                    {"capture", tx.getProperty("capture").toString().toStdString()}});
        }
        return {{"cursor", c.historyCursor},
                {"transactions", transactions},
                {"undo_description", c.edit->getUndoManager().getUndoDescription().toStdString()}};
    }
    static Commands& owner(Workspace& w)
    {
        return w.commands;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
void check(bool yes, const char* why)
{
    if (!yes)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void wait(F ready)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 4000;
    while (!ready() && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(ready(), "production callback completed within deadline");
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
template <class F> void click(Workspace& w, const juce::String& id, F ready)
{
    auto* b = dynamic_cast<juce::Button*>(find(w, id));
    check(b && b->isEnabled(), "real enabled GUI command exists");
    b->triggerClick();
    wait(ready);
}
juce::File fixture(const juce::File& dir, const juce::String& name, double frequency)
{
    auto path = dir.getChildFile(name + ".wav");
    juce::AudioBuffer<float> pcm(2, 144000);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < pcm.getNumSamples(); ++i)
            pcm.setSample(ch, i, float(.02 * std::sin(juce::MathConstants<double>::twoPi * frequency * i / 48000)));
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out = path.createOutputStream();
    auto writer = wav.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "actual file PCM created");
    return path;
}
void resize(juce::Component& c, int dx)
{
    auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto now = juce::Time::getCurrentTime();
    juce::MouseEvent down(mouse, {3.f, 30.f}, juce::ModifierKeys::leftButtonModifier, 0, 0, 0, 0, 0, &c, &c, now,
                          {3.f, 30.f}, now, 1, false);
    juce::MouseEvent drag(mouse, {float(3 + dx), 30.f}, juce::ModifierKeys::leftButtonModifier, 0, 0, 0, 0, 0, &c, &c,
                          now, {3.f, 30.f}, now, 1, true);
    const auto target = drag.getScreenPosition();
    c.mouseDown(down);
    c.mouseDrag(drag);
    const auto local = c.getLocalPoint(nullptr, target).toFloat();
    juce::MouseEvent up(mouse, local, {}, 0, 0, 0, 0, 0, &c, &c, now, {3.f, 30.f}, now, 1, true);
    c.mouseUp(up);
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("forma-feedback-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        auto a = fixture(dir, "birds", 437), b = fixture(dir, "wind", 659);
        const auto ha = Commands::mediaHash(a), hb = Commands::mediaHash(b);
        Workspace w(false);
        w.setSize(1440, 1000);
        w.setVisible(true);
        auto& c = AudioDeviceTestAccess::owner(w);
        w.importAudioFiles({a, b});
        auto imported = w.query()["tracks"];
        check(imported.size() == 2 && imported[0]["clips"].size() == 1 && imported[1]["clips"].size() == 1,
              "batch import creates one real audio track per file at the same cursor");
        click(w, "history.undo", [&] { return w.query()["tracks"].empty(); });
        click(w, "history.redo", [&] { return w.query()["tracks"] == imported; });
        const auto before = w.query();
        w.importAudioFiles({a, dir.getChildFile("missing.wav")});
        check(w.query() == before, "one invalid batch member rejects the complete import without partial tracks");
        const auto clip = imported[0]["clips"][0]["id"].get<std::string>();
        auto* area = find(w, "edit.timeline");
        check(area, "native timeline present");
        const auto height = area->getHeight();
        click(w, "clip.select:" + juce::String(clip), [&] { return !w.queryView()["object_selection"].empty(); });
        check(!find(w, "clip.trim") && !find(w, "clip.gain") && area->getHeight() == height,
              "selecting audio keeps full timeline height and does not open the removed numeric form");
        auto* divider = find(w, "workspace.inspector.divider");
        check(divider, "inspector resize handle present");
        resize(*divider, -80);
        check(w.queryView()["workspace_panes"]["inspector_width"] == 412,
              "inspector width is changed by a native drag through L1 view persistence");
        divider = find(w, "workspace.tracks.divider");
        check(w.getComponentAt(divider->getBounds().getCentre()) == divider,
              "track browser does not cover the actual mouse hit region of its resize handle");
        resize(*divider, 50);
        check(w.queryView()["workspace_panes"]["tracks_width"] == 188, "track browser width is freely draggable");
        auto legacy = w.queryView();
        legacy["ui_schema"] = 13;
        legacy.erase("workspace_panes");
        juce::ValueTree meta("NATIVEDAW"), ui("UI");
        ui.setProperty("json", text(legacy.dump()), nullptr);
        meta.addChild(ui, -1, nullptr);
        const auto migrated = readUiState(meta);
        check(migrated["ui_schema"] == 14 && migrated["workspace_panes"]["inspector_width"] == 332 &&
                  migrated["object_selection"] == legacy["object_selection"],
              "schema13 projects migrate to default pane widths while preserving actual selection");
        const auto view = w.queryView();
        auto* timeline = dynamic_cast<EditWindow*>(area);
        auto mouse = juce::Desktop::getInstance().getMainMouseSource();
        auto now = juce::Time::getCurrentTime();
        juce::MouseEvent gesture(mouse, {700.f, 220.f}, {}, 0, 0, 0, 0, 0, timeline, timeline, now, {700.f, 220.f}, now,
                                 1, false);
        timeline->mouseMagnify(gesture, 1.5f);
        check(w.queryView()["span_samples"].get<int64_t>() < view["span_samples"].get<int64_t>(),
              "native pinch gesture zooms the actual timeline around the pointer");
        const auto saved = dir.getChildFile("session.tracktionedit");
        c.save(saved);
        const auto final = w.query()["tracks"];
        const auto finalView = w.queryView();
        w.openSession(saved);
        check(w.query()["tracks"] == final && w.queryView()["workspace_panes"] == finalView["workspace_panes"] &&
                  w.queryView()["span_samples"] == finalView["span_samples"],
              "batch media and resized/zoomed workspace survive native save and reopen");
        {
            Workspace external(true);
            external.setVisible(true);
            auto& ec = AudioDeviceTestAccess::owner(external);
            external.importAudioFiles({a});
            wait([&] { return ec.deviceStatus().value("driver_running", false); });
            external.showPluginLibrary();
            wait([&] { return !external.queryPluginLibrary()["busy"].get<bool>(); });
            std::string au;
            const auto inventory = external.queryPluginLibrary();
            for (const auto& row : inventory["rows"])
                if (row["name"] == "AUNBandEQ" && row["status"] == "verified")
                    au = row["descriptor"];
            check(!au.empty() && external.selectLibraryPlugin(au),
                  "actual verified AU selected for batch history regression");
            click(external, "plugin.library.preview",
                  [&] { return external.query()["tracks"][0]["plugins"].size() == 1; });
            const auto auSaved = dir.getChildFile("au-import.tracktionedit");
            click(external, "plugin.editor", [&] { return external.queryPluginEditors().size() == 1; });
            click(external, "plugin.editor", [&] { return external.queryPluginEditors().empty(); });
            ec.save(auSaved);
            external.openSession(argc > 2 ? juce::File(argv[2]) : auSaved);
            const auto beforeBatch = external.query()["tracks"];
            auto observe = [&]
            {
                auto frames = ec.deviceStatus()["output_frames"].get<uint64_t>();
                // Observe delayed SDK notifications across 0.5 seconds of real callbacks,
                // rather than treating a short message-loop sleep as completion.
                wait([&] { return ec.deviceStatus()["output_frames"].get<uint64_t>() >= frames + 24000; });
            };
            observe();
            std::cout << "AU_HISTORY_BEFORE=" << AudioDeviceTestAccess::history(ec).dump() << std::endl;
            external.importAudioFiles({a, b});
            std::cout << "AU_HISTORY_IMMEDIATE=" << AudioDeviceTestAccess::history(ec).dump() << std::endl;
            observe();
            auto importedBatch = external.query()["tracks"];
            std::cout << "AU_HISTORY_SETTLED=" << AudioDeviceTestAccess::history(ec).dump() << std::endl;
            check(importedBatch.size() == beforeBatch.size() + 2,
                  "batch import with actual AU creates exactly two tracks");
            const auto importedHistory = external.query()["history"]["undo"];
            check(importedHistory["commands"] ==
                      Json::array({"track.create", "clip.import", "track.create", "clip.import"}),
                  "actual next Undo identifies the single two-file import Plan");
            auto* undo = dynamic_cast<juce::Button*>(find(external, "history.undo"));
            check(undo && undo->getTooltip().contains("2 个文件"), "native Undo tooltip describes the actual batch");
            const auto target = importedBatch[0]["id"].get<std::string>();
            auto* fader = dynamic_cast<juce::Slider*>(find(external, "track.gain:" + juce::String(target)));
            check(fader && fader->isEnabled(),
                  "actual original-track volume control remains available after importing");
            fader->setValue(-3, juce::sendNotificationSync);
            const auto editedBatch = external.query()["tracks"];
            check(editedBatch[0]["gain_db"] != importedBatch[0]["gain_db"] &&
                      external.query()["history"]["undo"]["commands"] == Json::array({"track.gain"}) &&
                      undo->getTooltip().contains("轨道音量"),
                  "later real human fader edit is separate and identified rather than silently merged into import");
            click(external, "history.undo", [&] { return external.query()["tracks"] == importedBatch; });
            check(external.query()["tracks"].size() == importedBatch.size() &&
                      external.query()["history"]["redo"]["commands"] == Json::array({"track.gain"}),
                  "first Undo restores only later volume edit and keeps all imported audio");
            click(external, "history.undo", [&] { return external.query()["tracks"] == beforeBatch; });
            observe();
            check(external.query()["tracks"] == beforeBatch,
                  "one Undo with actual AU removes the entire batch and retains plugin state");
            click(external, "history.redo", [&] { return external.query()["tracks"] == importedBatch; });
            click(external, "history.redo", [&] { return external.query()["tracks"] == editedBatch; });
            const auto retained = dir.getChildFile("au-batch-gain-final.tracktionedit");
            ec.save(retained);
            external.openSession(retained);
            check(external.query()["tracks"] == editedBatch &&
                      external.query()["history"]["undo"]["commands"] == Json::array({"track.gain"}),
                  "AU, imported media and separate fader value reopen with the actual persisted Undo transaction");
        }
        Workspace live(true);
        live.setVisible(true);
        auto& lc = AudioDeviceTestAccess::owner(live);
        live.importAudioFiles({a});
        wait([&] { return lc.deviceStatus().value("driver_running", false); });
        Json stops = Json::array();
        for (int start : {0, 48000, 96000})
        {
            lc.seek(start);
            click(
                live, "transport.play", [&]
                { return lc.query()["playing"].get<bool>() && lc.deviceStatus()["output_peak"].get<double>() > .001; });
            click(live, "transport.stop", [&] { return !lc.query()["playing"].get<bool>(); });
            wait([&] { return lc.deviceStatus()["output_peak"].get<double>() == 0; });
            const auto frames = lc.deviceStatus()["output_frames"].get<uint64_t>();
            wait([&] { return lc.deviceStatus()["output_frames"].get<uint64_t>() >= frames + 24000; });
            auto status = lc.deviceStatus();
            check(status["output_peak"] == 0,
                  "real device callback output stays silent for 0.5 seconds after GUI Stop");
            stops.push_back(status);
        }
        check(Commands::mediaHash(a) == ha && Commands::mediaHash(b) == hb, "source media retained unchanged");
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"device_stop_measurements", stops},
                    {"scope", "real PCM import, L1 atomic history, native workspace gestures and save/reopen; real "
                              "hardware callbacks, not acoustic loopback or subjective listening"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
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
