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
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("forma-presentation-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        auto media = dir.getChildFile("source.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
            auto writer = format.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(bool(writer), "real source writer created");
            juce::AudioBuffer<float> pcm(2, 48000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    pcm.setSample(ch, i, float(.04 * std::sin(i * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
            check(writer->writeFromAudioSampleBuffer(pcm, 0, 48000), "real one-second source written");
        }
        const auto hash = Commands::mediaHash(media);
        c.commit(c.makePlan(
            "human",
            Json::array(
                {op("track.create", {{"name", "Vocal"}, {"ref", "$v"}}),
                 op("clip.import",
                    {{"track", "$v"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 48000}}),
                 op("track.create", {{"name", "Second audio"}, {"ref", "$a"}}),
                 op("clip.import",
                    {{"track", "$a"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 96000}}),
                 op("track.create", {{"name", "Keys"}, {"type", "midi"}, {"ref", "$m"}}),
                 op("midi.clip.create", {{"track", "$m"},
                                         {"ref", "$clip"},
                                         {"name", "Notes"},
                                         {"position_samples", 0},
                                         {"length_samples", 96000}}),
                 op("midi.note.add", {{"clip", "$clip"},
                                      {"pitch", 60},
                                      {"velocity", 90},
                                      {"position_samples", 24000},
                                      {"length_samples", 12000}}),
                 op("track.create", {{"name", "Room"}, {"type", "aux"}, {"ref", "$room"}}),
                 op("track.create", {{"name", "Folder"}, {"type", "folder"}, {"ref", "$folder"}}),
                 op("track.create", {{"name", "VCA"}, {"type", "vca"}, {"ref", "$vca"}})})));
        pump();
        const auto initial = w.query();
        const auto id = initial["tracks"][0]["id"].get<std::string>();
        const auto second = initial["tracks"][1]["id"].get<std::string>(),
                   midi = initial["tracks"][2]["id"].get<std::string>();
        auto* edit = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        auto select = [&](const std::string& target)
        {
            auto* b = dynamic_cast<juce::Button*>(find(w, "track.select:" + text(target)));
            check(b != nullptr, "real track selector found");
            b->triggerClick();
            pump();
        };
        check(w.queryView()["ui_schema"] == 14 && w.queryView()["track_heights"].empty() &&
                  w.queryView()["zoom_presets"].size() == 5,
              "new UI defaults have seven-schema sparse heights and five usable presets");
        select(id);
        invoke(w, 210);
        check(edit->rowHeight(0) == 32 && edit->rowY(1) == edit->rulerHeight() + 32 &&
                  edit->rowY(2) == edit->rulerHeight() + 176,
              "prefix axis uses individual stable track heights");
        check(!find(w, "track.gain:" + text(id)) && !find(w, "track.mute:" + text(id)) &&
                  find(w, "track.options:" + text(id)),
              "Micro hides controls that cannot fit and keeps actual options");
        select(second);
        invoke(w, 214);
        check(edit->rowHeight(1) == 224 && edit->rowY(2) == edit->rulerHeight() + 256,
              "second track height does not resize first or third track");
        const auto original = initial["tracks"][1]["clips"][0];
        auto point = edit->clipRect(original, 1).getCentre().toFloat();
        const auto shift = std::llround(20 * edit->coordinates().span / edit->coordinates().width);
        drag(*edit, point, point + juce::Point<float>(20, 0));
        check(w.query()["tracks"][1]["clips"][0]["start_samples"] == original["start_samples"].get<int64_t>() + shift,
              "real audio drag hits correct row after nonuniform heights");
        invoke(w, 6);
        check(w.query()["tracks"][1]["clips"] == initial["tracks"][1]["clips"],
              "one Undo restores exact real clip after variable-height gesture");
        invoke(w, editCommand::selector);
        const auto axis = edit->coordinates();
        drag(*edit, {float(axis.pixelAt(12000)), float(edit->rowY(0) + 16)},
             {float(axis.pixelAt(36000)), float(edit->rowY(2) + 40)});
        check(w.queryView()["selection_tracks"] == Json::array({id, second, midi}),
              "cross-track range uses prefix-row hit testing including compressed row");
        invoke(w, editCommand::grabber);
        select(midi);
        invoke(w, 212);
        auto* header = dynamic_cast<TrackHeader*>(find(w, "track.select:" + text(midi))->getParentComponent());
        const auto beforeResize = w.queryView();
        const auto domainBeforeResize = w.query();
        const int resizedHeight = edit->rowHeight(2) + 37;
        const auto bottom = juce::Point<float>(200, float(header->getHeight() - 2));
        header->mouseDown(event(*header, bottom));
        header->mouseDrag(event(*header, bottom + juce::Point<float>(0, 37), true));
        check(w.queryView() == beforeResize && edit->rowHeight(2) == resizedHeight,
              "native header drag previews height without writing Edit");
        header->mouseUp(event(*header, bottom + juce::Point<float>(0, 37), true));
        pump();
        check(w.queryView()["track_heights"][midi] == resizedHeight && w.query() == domainBeforeResize,
              "resize release writes only L1 UI height and preserves domain Undo/revision");
        point = edit->clipRect(w.query()["tracks"][2]["clips"][0], 2).getCentre().toFloat();
        edit->mouseDoubleClick(event(*edit, point));
        pump();
        check(find(w, "midi.editor") != nullptr &&
                  w.queryView()["midi_clip"] == w.query()["tracks"][2]["clips"][0]["id"],
              "MIDI double-click resolves true clip under variable-height rows");
        invoke(w, 145);
        // A coordinate-changing UI operation must cancel an existing draft, including header resizes.
        const auto hpoint = juce::Point<float>(200, float(header->getHeight() - 2));
        header->mouseDown(event(*header, hpoint));
        header->mouseDrag(event(*header, hpoint + juce::Point<float>(0, 20), true));
        invoke(w, 170);
        const auto cancelled = w.queryView();
        header->mouseUp(event(*header, hpoint + juce::Point<float>(0, 20), true));
        pump();
        check(w.queryView() == cancelled && edit->rowHeight(2) == 144,
              "height command cancels in-flight drag without stale release overwrite");
        const auto firstHeight = edit->rowHeight(0), secondHeight = edit->rowHeight(1);
        invoke(w, 172);
        check(edit->rowHeight(0) == juce::roundToInt(firstHeight * 1.25) &&
                  edit->rowHeight(1) == juce::roundToInt(secondHeight * 1.25),
              "all-track vertical zoom applies proportional heights");
        select(id);
        c.updateUiState({{"selection_tracks", Json::array({id, second})}}, c.sessionToken());
        pump();
        invoke(w, 213);
        check(edit->rowHeight(0) == 144 && edit->rowHeight(1) == 144,
              "selected-track height command applies shared stable selection");
        const auto beforeColour = w.query();
        const auto beforeFile = dir.getChildFile("before.wav"), afterFile = dir.getChildFile("after.wav");
        c.render(beforeFile, 0, 96000);
        invoke(w, 202);
        auto coloured = w.query();
        check(coloured["revision"] == beforeColour["revision"].get<uint64_t>() + 1 &&
                  coloured["tracks"][0]["colour"] == "#A77BD7" && coloured["tracks"][1]["colour"] == "#A77BD7",
              "shared colour command changes selected tracks in one actual Undo transaction");
        invoke(w, 6);
        check(w.query()["tracks"] == beforeColour["tracks"], "one Undo restores all selected colours");
        invoke(w, 7);
        check(w.query()["tracks"] == coloured["tracks"], "Redo restores all native colours");
        invoke(w, 100);
        auto* mixName = dynamic_cast<juce::Button*>(find(w, "track.select:" + text(id)));
        check(mixName && mixName->findColour(juce::TextButton::buttonColourId) ==
                             trackColour(coloured["tracks"][0]).darker(.45f),
              "actual Mix header colour reads same native track property");
        invoke(w, 100);
        auto* editName = dynamic_cast<juce::Button*>(find(w, "track.select:" + text(id)));
        check(editName && editName->findColour(juce::TextButton::buttonColourId) ==
                              trackColour(coloured["tracks"][0]).darker(.45f),
              "actual Edit header colour matches restored native track property");
        c.render(afterFile, 0, 96000);
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        auto beforeReader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(beforeFile)),
             afterReader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(afterFile));
        juce::AudioBuffer<float> beforePcm(2, 96000), afterPcm(2, 96000);
        check(beforeReader && afterReader && beforeReader->read(&beforePcm, 0, 96000, 0, true, true) &&
                  afterReader->read(&afterPcm, 0, 96000, 0, true, true),
              "actual rendered PCM files read");
        double error = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 96000; ++i)
                error = std::max(error, std::abs(double(beforePcm.getSample(ch, i) - afterPcm.getSample(ch, i))));
        check(beforePcm.getMagnitude(0, 0, 96000) > .01 && error == 0,
              "height and colour preserve actual nonzero rendered PCM exactly");
        // Generated menu items are deferred: one dispatcher only, so toggles cannot run twice.
        const auto menuSnapshot = w.query(), viewSnapshot = w.queryView();
        for (int index = 0; index < 4; ++index)
        {
            auto menu = w.getMenuForIndex(index, "");
            juce::PopupMenu::MenuItemIterator it(menu, true);
            while (it.next())
                check(it.getItem().commandManager == nullptr,
                      "menu generation defers actual command dispatch until guarded completion");
        }
        check(w.query() == menuSnapshot && w.queryView() == viewSnapshot,
              "reading command/menu state never mutates project or layout");
        const auto oldIo = w.queryView()["edit_views"]["io"].get<bool>();
        w.menuItemSelected(146, 2);
        pump();
        check(w.queryView()["edit_views"]["io"] == !oldIo, "one menu completion toggles exactly once");
        c.updateUiState({{"start_samples", 12000}, {"span_samples", 123456}}, c.sessionToken());
        c.seek(36000);
        pump();
        invoke(w, 186);
        check(w.queryView()["zoom_presets"][1] == 123456, "save preset captures actual horizontal span");
        c.updateUiState({{"span_samples", 200000}}, c.sessionToken());
        pump();
        const auto zoomDomain = w.query();
        invoke(w, 181);
        check(w.queryView()["span_samples"] == 123456 && w.query() == zoomDomain,
              "recall exact horizontal preset leaves domain revision and Undo unchanged");
        const auto beforeBad = w.queryView();
        rejects([&] { c.updateUiState({{"track_heights", {{id, 31}}}}, c.sessionToken()); },
                "undersized track height rejected");
        rejects([&] { c.updateUiState({{"track_heights", {{id, 641}}}}, c.sessionToken()); },
                "oversized track height rejected");
        rejects([&] { c.updateUiState({{"track_heights", {{id, 96.5}}}}, c.sessionToken()); },
                "fractional height rejected");
        rejects([&]
                { c.updateUiState({{"zoom_presets", Json::array({48000, 96000, 144000, 192000})}}, c.sessionToken()); },
                "incomplete preset array rejected");
        rejects(
            [&]
            { c.updateUiState({{"zoom_presets", Json::array({0, 48000, 96000, 144000, 192000})}}, c.sessionToken()); },
            "zero zoom span rejected");
        check(w.queryView() == beforeBad, "bad presentation properties cannot partially replace valid UI state");
        auto old = beforeBad;
        old["ui_schema"] = 6;
        old.erase("workspace_panes");
        old.erase("track_heights");
        old.erase("zoom_presets");
        old.erase("track_views");
        old.erase("zoom_state");
        old.erase("waveform_zoom");
        old.erase("midi_zoom");
        old.erase("zoom_toggle");
        old.erase("midi_note_height");
        juce::ValueTree metadata("NATIVEDAW"), ui("UI");
        ui.setProperty("json", text(old.dump()), nullptr);
        metadata.addChild(ui, -1, nullptr);
        const auto migrated = readUiState(metadata);
        check(migrated["ui_schema"] == 14 && migrated["rulers"] == old["rulers"] &&
                  migrated["edit_views"] == old["edit_views"] && migrated["track_heights"].empty(),
              "complete schema6 migrates existing rulers and columns without invented height overrides");
        old.erase("row_height");
        ui.setProperty("json", text(old.dump()), nullptr);
        rejects([&] { readUiState(metadata); }, "incomplete old UI rejected");
        auto* keys = w.uiCommands().getKeyMappings();
        keys->clearAllKeyPresses(170);
        const juce::KeyPress custom('j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 'j');
        keys->addKeyPress(170, custom);
        pump();
        w.setSize(1120, 700);
        pump();
        auto* presets = find(w, "zoom.presets");
        auto* controls = find(w, "edit.controls");
        check(presets && controls && presets->getBounds().getRight() < controls->getX(),
              "five native preset buttons fit minimum window before editing tools");
        const auto session = dir.getChildFile("presentation.tracktionedit");
        c.save(session);
        const auto saved = w.query(), savedUi = w.queryView();
        Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("reopen")));
        reopened.setVisible(true);
        reopened.setSize(1120, 700);
        reopened.openSession(session);
        pump();
        check(reopened.query()["tracks"] == saved["tracks"],
              "new Workspace reopen retains native colours clips MIDI and routing");
        check(reopened.queryView()["track_heights"] == savedUi["track_heights"] &&
                  reopened.queryView()["zoom_presets"] == savedUi["zoom_presets"],
              "new Workspace reopen restores exact per-track heights and five edited presets");
        check(reopened.uiCommands().getKeyMappings()->containsMapping(170, custom),
              "custom presentation keyboard binding survives reopen");
        const auto reopenedDomain = reopened.query();
        check(reopened.keyPressed(custom), "restored custom height key invokes actual presentation command");
        pump();
        check(reopened.queryView()["track_heights"][id] == 224 && reopened.query() == reopenedDomain,
              "custom height key changes persisted selected track view without a domain transaction");
        check(Commands::mediaHash(media) == hash, "original source remains unmodified by UI and colour edits");
        Json report{
            {"result", "passed"},
            {"checks", checks},
            {"ui_schema", 14},
            {"media_sha256", hash},
            {"render_peak", beforePcm.getMagnitude(0, 0, 96000)},
            {"render_maximum_error", error},
            {"scope", "actual components and Tracktion Edit; closed audio device; desktop acceptance unexecuted"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
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
