// SPDX-License-Identifier: AGPL-3.0-only
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
    static Commands& owner(Workspace& w)
    {
        return w.commands;
    }
    static void select(Workspace& w, const std::string& t)
    {
        w.refresh();
        w.select(t);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, renders = 0;
double copiedError = 0, partialError = 0, nestedError = 0;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
class Storage final : public te::PropertyStorage
{
    juce::File dir;

public:
    Storage(juce::File f) : PropertyStorage("Forma crossfade clipboard tests"), dir(f) {}
    juce::File getAppPrefsFolder() override
    {
        dir.createDirectory();
        return dir;
    }
};
Json op(const char* cmd, Json args)
{
    return {{"command", cmd}, {"args", args}};
}
Json run(Commands& c, const char* cmd, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(cmd, a)})));
}
Json pieces(Commands& c, const std::string& t)
{
    const auto facts = c.query();
    for (const auto& track : facts["tracks"])
        if (track["id"] == t)
            return track["clips"];
    throw std::runtime_error("track missing");
}
Json piece(Commands& c, const std::string& t, const Json& id)
{
    for (auto p : pieces(c, t))
        if (p["id"] == id)
            return p;
    throw std::runtime_error("clip missing");
}
void fade(Commands& c, const std::string& t, int64_t join, const char* curve)
{
    auto target = c.crossfadeTarget(t, join);
    run(c, "playlist.crossfade",
        {{"track", t},
         {"left", target["left"]},
         {"right", target["right"]},
         {"start_samples", join - 2400},
         {"end_samples", join + 2400},
         {"curve", curve}});
}
Json copyAudio(Commands& c, const std::string& t, int64_t first, int64_t last)
{
    Json slices = Json::array();
    for (auto clip : pieces(c, t))
    {
        int64_t low = std::max(first, clip["start_samples"].get<int64_t>());
        int64_t high = std::min(last, clip["start_samples"].get<int64_t>() + clip["length_samples"].get<int64_t>());
        if (high > low)
            slices.push_back({{"clip", clip["id"]}, {"start_samples", low}, {"end_samples", high}});
    }
    return c.prepareClipboard(slices, Json::array({t}), first, last, c.sessionToken(), c.query()["revision"]);
}
Json pasteTimeline(Commands& c, const Json& b, const std::string& t, int64_t point, const char* mode = "overlay")
{
    return c.makePlan(
        "human",
        Json::array(
            {op("timeline.clips.paste",
                {{"clipboard", b["id"]}, {"tracks", Json::array({t})}, {"position_samples", point}, {"mode", mode}})}));
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, int64_t end)
{
    auto file = root.getChildFile("clipboard-render-" + juce::String(++renders) + ".wav");
    auto receipt = c.render(file, 0, end);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    check(receipt["frames"] == end && reader && reader->sampleRate == 48000 && reader->numChannels == 2,
          "actual native WAV format, length and channels verified");
    juce::AudioBuffer<float> pcm(2, int(end));
    check(reader->read(&pcm, 0, int(end), 0, true, true), "actual PCM decoded");
    return pcm;
}
double compare(const juce::AudioBuffer<float>& a, int firstA, const juce::AudioBuffer<float>& b, int firstB, int length)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i)
            error = std::max(error, std::abs(double(a.getSample(ch, firstA + i)) - b.getSample(ch, firstB + i)));
    check(error < 2e-7, "copied actual PCM retains its original source phase on both channels");
    return error;
}
std::pair<std::string, std::string> seed(Commands& c, const juce::File& media)
{
    c.commit(c.makePlan("human", Json::array({op("track.create", {{"name", "Source"}, {"ref", "$s"}}),
                                              op("clip.import", {{"track", "$s"},
                                                                 {"path", media.getFullPathName().toStdString()},
                                                                 {"position_samples", 0}}),
                                              op("track.create", {{"name", "Destination"}, {"ref", "$d"}})})));
    auto ts = c.query()["tracks"];
    return {ts[0]["id"], ts[1]["id"]};
}
void pairedCopies(const juce::File& root, const juce::File& media)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("pairs-prefs")));
    const auto [s, t] = seed(c, media);
    const std::string left = pieces(c, s)[0]["id"];
    run(c, "clip.split", {{"clip", left}, {"position_samples", 48000}, {"ref", "$r"}});
    fade(c, s, 48000, "equal_power");
    auto reference = render(c, root, 96000);
    auto buffer = copyAudio(c, s, 0, 96000);
    check(buffer["crossfades"].size() == 1, "audio clipboard freezes the real complete pair");
    c.acceptClipboard(buffer["id"]);
    fade(c, s, 48000, "linear");
    auto plan = c.makeClipboardPastePlan(buffer["id"], Json::array({t}), 144000, 240000, "replace");
    auto preview = c.preview(plan);
    check(preview["crossfade_changes"].size() == 1 && preview["crossfade_changes"][0]["state"] == "copied" &&
              c.crossfades(t).empty(),
          "paste dry-run declares new pair identity and does not edit target");
    auto receipt = c.commit(plan);
    auto made = c.crossfades(t);
    check(made.size() == 1 && made[0]["curve"] == "equal_power" && made[0]["start_samples"] == 189600 &&
              made[0]["left"] != left && c.crossfades(s)[0]["curve"] == "linear",
          "paste binds new clips and destination Playlist to the frozen curve, preserving later human source edit");
    check(c.commit(plan)["replayed"] == true && c.crossfades(t) == made,
          "idempotent retry does not duplicate paired paste");
    auto pcm = render(c, root, 240000);
    copiedError = std::max(copiedError, compare(pcm, 144000, reference, 0, 96000));
    c.undo(receipt["plan_id"]);
    check(c.crossfades(t).empty() && pieces(c, t).empty() && c.crossfades(s)[0]["curve"] == "linear",
          "one Undo removes pasted clips and pair without erasing later source history");
    c.redo();
    check(c.crossfades(t) == made, "Redo restores exact new transition identity");
    const auto file = root.getChildFile("Paired-Paste.tracktionedit");
    c.save(file);
    {
        Commands opened(false, std::make_unique<Storage>(root.getChildFile("pairs-open")));
        opened.open(file);
        check(opened.crossfades(t) == made, "paired paste stable ID and actual native geometry survive reopen");
        opened.undo();
        check(opened.crossfades(t).empty(), "reopened Undo removes complete paste transaction");
        opened.redo();
        check(opened.crossfades(t) == made, "reopened Redo restores complete paste transaction");
        run(opened, "playlist.crossfade.remove",
            {{"track", t}, {"left", made[0]["left"]}, {"right", made[0]["right"]}});
        auto ps = pieces(opened, t);
        check(ps[0]["start_samples"] == 144000 && ps[0]["length_samples"] == 48000 &&
                  ps[1]["start_samples"] == 192000 && ps[1]["source_offset_samples"] == 48000,
              "remove pasted fade restores translated core edges and real source offsets");
    }
    fade(c, s, 48000, "equal_power");
    auto range =
        c.prepareTimelineRangeClipboard(Json::array({s}), 42000, 56000, c.sessionToken(), c.query()["revision"]);
    check(range["crossfades"].size() == 1, "partial outer crop freezes the complete retained transition");
    c.acceptClipboard(range["id"]);
    auto rp = pasteTimeline(c, range, t, 288000);
    auto rr = c.commit(rp);
    auto pairs = c.crossfades(t);
    Json cropped = nullptr;
    for (auto p : pairs)
        if (p["start_samples"] == 291600)
            cropped = p;
    check(!cropped.is_null() && pairs.size() == 2, "timeline clipboard creates a translated independent cropped pair");
    pcm = render(c, root, 302000);
    copiedError = std::max(copiedError, compare(pcm, 288000, reference, 42000, 14000));
    const auto partial = root.getChildFile("Cropped-Pair.tracktionedit");
    c.save(partial);
    {
        Commands opened(false, std::make_unique<Storage>(root.getChildFile("cropped-open")));
        opened.open(partial);
        opened.undo();
        check(opened.crossfades(t) == made, "cropped paste reopened Undo leaves earlier full paste intact");
        opened.redo();
        check(opened.crossfades(t) == pairs, "cropped paste reopened Redo restores all stable pair IDs");
        run(opened, "playlist.crossfade.remove",
            {{"track", t}, {"left", cropped["left"]}, {"right", cropped["right"]}});
        auto a = piece(opened, t, cropped["left"]), b = piece(opened, t, cropped["right"]);
        check(a["start_samples"] == 288000 && a["length_samples"] == 6000 && a["source_offset_samples"] == 42000 &&
                  b["start_samples"] == 294000 && b["length_samples"] == 8000 && b["source_offset_samples"] == 48000,
              "cropped restoration retains crop edges rather than restoring the full original clips");
    }
    for (bool timeline : {false, true})
    {
        auto b = timeline ? c.prepareTimelineRangeClipboard(Json::array({s}), 47000, 49000, c.sessionToken(),
                                                            c.query()["revision"])
                          : copyAudio(c, s, 47000, 49000);
        check(b["crossfades"].empty() && b["unpaired_crossfades"].size() == 1,
              "cut-through-transition copy declares retained envelopes without fabricating a complete pair");
        c.acceptClipboard(b["id"]);
        const int point = timeline ? 400000 : 320000;
        auto p = timeline ? pasteTimeline(c, b, t, point)
                          : c.makeClipboardPastePlan(b["id"], Json::array({t}), point, point + 2000, "overlay");
        c.commit(p);
        pcm = render(c, root, point + 2000);
        partialError = std::max(partialError, compare(pcm, point, reference, 47000, 2000));
        check(c.crossfades(t) == pairs, "partial ramp paste preserves earlier pair objects and creates no false pair");
    }
    auto audio = copyAudio(c, s, 42000, 56000);
    c.acceptClipboard(audio["id"]);
    auto adapted = c.prepareTimelineFromAudioClipboard(audio["id"], c.sessionToken(), c.query()["revision"]);
    auto adaptedReceipt = c.commit(pasteTimeline(c, adapted, t, 480000));
    check(c.crossfades(t).size() == 3, "audio-to-timeline adaptation retains frozen pair with new clipboard tokens");
    c.acceptClipboard(adapted["id"]);
    pcm = render(c, root, 494000);
    copiedError = std::max(copiedError, compare(pcm, 480000, reference, 42000, 14000));
    const auto beforeShuffle = c.crossfades(t), beforeClips = pieces(c, t);
    auto full = copyAudio(c, s, 0, 96000);
    c.acceptClipboard(full["id"]);
    auto shifted = c.prepareTimelineFromAudioClipboard(full["id"], c.sessionToken(), c.query()["revision"]);
    auto shuffle = pasteTimeline(c, shifted, t, 96000, "shuffle");
    const auto shiftPreview = c.preview(shuffle);
    bool described = false;
    for (const auto& change : shiftPreview["crossfade_changes"])
        if (change["state"] == "moved" && change["id"] == made[0]["id"] && change["delta_samples"] == 96000)
            described = true;
    check(described, "Shuffle preview declares the existing pair's common sample displacement");
    c.commit(shuffle);
    auto shiftedPairs = c.crossfades(t);
    bool retained = false;
    for (const auto& pair : shiftedPairs)
        if (pair["id"] == made[0]["id"] && pair["start_samples"] == made[0]["start_samples"].get<int64_t>() + 96000)
            retained = true;
    check(retained && shiftedPairs.size() == beforeShuffle.size() + 1,
          "Shuffle preserves existing paired IDs and moves their restoration anchors with the suffix");
    const auto shuffled = root.getChildFile("Shuffled-Pairs.tracktionedit");
    c.save(shuffled);
    {
        Commands opened(false, std::make_unique<Storage>(root.getChildFile("shuffle-open")));
        opened.open(shuffled);
        check(opened.crossfades(t) == shiftedPairs, "shifted and inserted pair objects validate after save/reopen");
        opened.undo();
        check(opened.crossfades(t) == beforeShuffle && pieces(opened, t) == beforeClips,
              "reopened one Undo restores every original suffix and paired restoration snapshot");
        opened.redo();
        check(opened.crossfades(t) == shiftedPairs, "reopened Redo preserves Shuffle pair identities");
    }
    c.undo();
    c.save(root.getChildFile("Clipboard-Pairs-Ready.tracktionedit"));
}
void nestedEnvelopes(const juce::File& root, const juce::File& media)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("envelope-prefs")));
    const auto [s, t] = seed(c, media);
    const std::string original = pieces(c, s)[0]["id"];
    run(c, "clip.fade",
        {{"clip", original},
         {"in_samples", 24000},
         {"out_samples", 24000},
         {"in_curve", "linear"},
         {"out_curve", "convex"}});
    auto reference = render(c, root, 96000);
    auto b = copyAudio(c, s, 12000, 20000);
    c.acceptClipboard(b["id"]);
    c.commit(c.makeClipboardPastePlan(b["id"], Json::array({t}), 120000, 128000, "overlay"));
    auto pcm = render(c, root, 128000);
    partialError = std::max(partialError, compare(pcm, 120000, reference, 12000, 8000));
    const std::string left = pieces(c, t)[0]["id"];
    run(c, "clip.split", {{"clip", left}, {"position_samples", 124000}, {"ref", "$r"}});
    fade(c, t, 124000, "equal_power");
    reference = render(c, root, 128000);
    b = copyAudio(c, t, 123500, 124500);
    c.acceptClipboard(b["id"]);
    auto receipt = c.commit(c.makeClipboardPastePlan(b["id"], Json::array({t}), 200000, 201000, "overlay"));
    pcm = render(c, root, 201000);
    nestedError = std::max(nestedError, compare(pcm, 200000, reference, 123500, 1000));
    const auto file = root.getChildFile("Nested-Envelopes.tracktionedit");
    c.save(file);
    Commands opened(false, std::make_unique<Storage>(root.getChildFile("envelope-open")));
    opened.open(file);
    auto reopened = render(opened, root, 201000);
    nestedError = std::max(nestedError, compare(reopened, 200000, reference, 123500, 1000));
    opened.undo();
    check(pieces(opened, t).size() == 2, "nested envelope copy is one reopened Undo transaction");
    opened.redo();
    check(pieces(opened, t).size() == 4, "nested envelope copy Redo restores both real fragments");
    std::string copiedLeft, copiedRight;
    for (auto o : receipt["objects"])
        if (o.contains("clipboard_token"))
            (copiedLeft.empty() ? copiedLeft : copiedRight) = o["id"].get<std::string>();
    opened.commit(opened.makePlan("human", Json::array({op("clip.fade", {{"clip", copiedLeft},
                                                                         {"in_samples", 0},
                                                                         {"out_samples", 0},
                                                                         {"in_curve", "linear"},
                                                                         {"out_curve", "linear"}}),
                                                        op("clip.delete", {{"clip", copiedRight}})})));
    check(!piece(opened, t, copiedLeft)["inherited_fades"].get<bool>(),
          "explicit fade edit clears every inherited layer");
    pcm = render(opened, root, 201000);
    double error = 0;
    for (int n = 200000; n < 201000; ++n)
        error = std::max(error, std::abs(double(pcm.getSample(0, n)) - .125));
    check(error < 2e-7, "actual audio confirms explicit fade reset removed all retained source ramps");
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* r = find(*child, id))
            return r;
    return nullptr;
}
juce::MouseEvent mouse(juce::Component& c, juce::Point<float> p, bool drag = false)
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
            drag};
}
void nativeCopyInteraction(const juce::File& root, const juce::File& media)
{
    Workspace w(false, std::make_unique<Storage>(root.getChildFile("clipboard-ui-prefs")));
    w.setSize(1440, 900);
    w.setVisible(true);
    auto& c = AudioDeviceTestAccess::owner(w);
    const auto [s, t] = seed(c, media);
    const std::string l = pieces(c, s)[0]["id"];
    run(c, "clip.split", {{"clip", l}, {"position_samples", 48000}, {"ref", "$r"}});
    fade(c, s, 48000, "equal_power");
    AudioDeviceTestAccess::select(w, s);
    w.uiCommands().invokeDirectly(editCommand::selector, false);
    auto* area = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
    check(area, "actual timeline exists for copy gesture");
    auto axis = area->coordinates();
    const float y = area->rowY(0) + 76;
    area->mouseDown(mouse(*area, {float(axis.pixelAt(0)), y}));
    area->mouseDrag(mouse(*area, {float(axis.pixelAt(96000)), y}, true));
    area->mouseUp(mouse(*area, {float(axis.pixelAt(96000)), y}, true));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    const auto revision = c.query()["revision"];
    check(w.uiCommands().invokeDirectly(editCommand::copy, false) && c.clipboard()["crossfades"].size() == 1 &&
              c.query()["revision"] == revision,
          "actual selector gesture and Copy command freeze one pair without editing");
    axis = area->coordinates();
    const float destinationY = area->rowY(1) + 76;
    area->mouseDown(mouse(*area, {float(axis.pixelAt(144000)), destinationY}));
    area->mouseUp(mouse(*area, {float(axis.pixelAt(144000)), destinationY}));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(w.uiCommands().invokeDirectly(editCommand::paste, false) && c.crossfades(t).size() == 1,
          "ordinary global Paste commits the small paired edit directly");
    const auto direct = c.crossfades(t);
    w.uiCommands().invokeDirectly(6, false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(c.crossfades(t).empty() && pieces(c, t).empty(), "one native Undo removes direct paired paste");
    w.uiCommands().invokeDirectly(7, false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(c.crossfades(t) == direct, "native Redo restores direct paired paste with stable identity");
    w.uiCommands().invokeDirectly(6, false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    Json imports = Json::array();
    for (int i = 0; i < 9; ++i)
        imports.push_back(
            op("clip.import",
               {{"track", t}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 144000}}));
    c.commit(c.makePlan("human", imports));
    const auto replaced = pieces(c, t);
    AudioDeviceTestAccess::select(w, t);
    axis = area->coordinates();
    area->mouseDown(mouse(*area, {float(axis.pixelAt(144000)), destinationY}));
    area->mouseUp(mouse(*area, {float(axis.pixelAt(144000)), destinationY}));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    const int64_t expectedStart = c.realtimeState().position + 45600 - c.clipboard()["start_samples"].get<int64_t>();
    check(w.uiCommands().invokeDirectly(editCommand::paste, false), "bulk replacement opens the actual native preview");
    auto* accept = dynamic_cast<juce::Button*>(find(w, "plan.accept"));
    check(accept && accept->isVisible() && accept->isEnabled() && c.crossfades(t).empty(),
          "preview accept is visible and target stays unchanged before acceptance");
    accept->triggerClick();
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 3000;
    while (c.crossfades(t).empty() && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    auto copied = c.crossfades(t);
    if (copied.size() != 1 || copied[0]["curve"] != "equal_power" || copied[0]["start_samples"] != expectedStart)
    {
        auto* status = dynamic_cast<juce::Label*>(find(w, "workspace.status"));
        std::cout << "GUI paste status=" << (status ? status->getText().toStdString() : "missing")
                  << " pairs=" << copied.dump() << " clips=" << pieces(c, t).size()
                  << " position=" << c.realtimeState().position << std::endl;
    }
    check(copied.size() == 1 && copied[0]["curve"] == "equal_power" && copied[0]["start_samples"] == expectedStart,
          "one actual acceptance inserts new paired fade at the timeline cursor");
    w.uiCommands().invokeDirectly(6, false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(c.crossfades(t).empty() && pieces(c, t) == replaced,
          "native global Undo removes paired paste and restores every replaced destination clip");
    w.uiCommands().invokeDirectly(7, false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(c.crossfades(t) == copied, "native global Redo restores exact copied pair identity");
    c.save(root.getChildFile("Crossfade-Clipboard-UI.tracktionedit"));
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("forma-crossfade-clipboard-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        auto media = root.getChildFile("source.wav");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = wav.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        juce::AudioBuffer<float> signal(2, 96000);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 96000; ++n)
                signal.setSample(ch, n, .125f);
        check(writer && writer->writeFromAudioSampleBuffer(signal, 0, 96000), "owned real PCM source written");
        writer.reset();
        auto hash = Commands::mediaHash(media);
        pairedCopies(root, media);
        nestedEnvelopes(root, media);
        nativeCopyInteraction(root, media);
        check(Commands::mediaHash(media) == hash, "source media hash is unchanged after every copy/edit/render");
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"renders", renders},
                    {"copy_pcm_error", copiedError},
                    {"partial_pcm_error", partialError},
                    {"nested_pcm_error", nestedError},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"scope", "real Tracktion Edit, native Undo, source media and decoded WAV; physical UI separate"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
