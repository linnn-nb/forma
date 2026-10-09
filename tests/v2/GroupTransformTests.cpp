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
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
Json measurements = Json::array();
void check(bool ok, const char* label)
{
    if (!ok)
        throw std::runtime_error(label);
    ++checks;
    std::cout << "PASS " << label << std::endl;
}
template <class F> void fails(F action, const char* label)
{
    bool refused = false;
    try
    {
        action();
    }
    catch (const std::exception&)
    {
        refused = true;
    }
    check(refused, label);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File dir) : PropertyStorage("Forma group transforms"), dir(dir) {}
    juce::File getAppPrefsFolder() override
    {
        dir.createDirectory();
        return dir;
    }

private:
    juce::File dir;
};
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
}
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", args}};
}
Json run(Commands& c, const char* command, Json args)
{
    return c.commit(c.makePlan("human", Json::array({op(command, args)})));
}
Json clip(Commands& c, size_t index)
{
    return c.query()["tracks"][index]["clips"][0];
}
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (!p.isVisible())
        return nullptr;
    if (p.getComponentID() == id)
        return &p;
    for (auto* child : p.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
void click(Workspace& w, const juce::String& id)
{
    auto* button = dynamic_cast<juce::Button*>(find(w, id));
    check(button && button->isEnabled(), "native action enabled");
    button->triggerClick();
    pump();
}
juce::File source(const juce::File& dir, int rate)
{
    auto file = dir.getChildFile("source-" + juce::String(rate) + ".wav");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = format.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "real PCM writer available");
    juce::AudioBuffer<float> pcm(2, rate * 4);
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < rate * 4; ++n)
            pcm.setSample(ch, n,
                          float(.06 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * n / rate)));
    check(writer->writeFromAudioSampleBuffer(pcm, 0, rate * 4), "actual four-second stereo source written");
    return file;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& dir, const char* label)
{
    auto file = dir.getChildFile(juce::Uuid().toString() + ".wav");
    auto receipt = c.render(file, 0, 192000);
    check(receipt["frames"] == 192000 && receipt["render_ms"].get<double>() <= 10000,
          "native render length and 10s budget");
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    check(reader && reader->numChannels == 2 && reader->sampleRate == 48000 && reader->lengthInSamples == 192000,
          "real exported WAV format decoded");
    juce::AudioBuffer<float> pcm(2, 192000);
    check(reader->read(&pcm, 0, 192000, 0, true, true), "actual rendered PCM decoded");
    measurements.push_back({{"case", label}, {"receipt", receipt}});
    return pcm;
}
double difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int start, int n,
                  double factor = 1.)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = start; i < start + n; ++i)
            error = std::max(error, std::abs(double(a.getSample(ch, i)) - double(b.getSample(ch, i)) * factor));
    return error;
}
void restore(Commands& c, const Json& receipt, const Json& before, const Json& after)
{
    pump();
    c.undo(receipt["plan_id"]);
    check(c.query()["tracks"] == before, "one native Undo restores all members and outsider");
    c.redo();
    check(c.query()["tracks"] == after, "native Redo restores exact member state");
    c.undo(receipt["plan_id"]);
    pump();
}
auto event(EditWindow& area, juce::Point<float> p, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p,
                            juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, &area, &area, now, p, now, 1,
                            dragged);
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("forma-group-transforms-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        auto aSource = source(dir, 48000), bSource = source(dir, 44100);
        const auto aHash = Commands::mediaHash(aSource), bHash = Commands::mediaHash(bSource);
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        Json ops = Json::array();
        for (const auto* name : {"A", "B", "C"})
        {
            std::string ref = std::string("$") + name;
            ops.push_back(op("track.create", {{"name", name}, {"ref", ref}}));
            ops.push_back(op("clip.import",
                             {{"track", ref},
                              {"path", (std::string(name) == "B" ? bSource : aSource).getFullPathName().toStdString()},
                              {"position_samples", 0}}));
        }
        c.commit(c.makePlan("human", ops));
        const auto aID = clip(c, 0)["id"].get<std::string>(), bID = clip(c, 1)["id"].get<std::string>();
        const auto aTrack = c.query()["tracks"][0]["id"], bTrack = c.query()["tracks"][1]["id"];
        run(c, "track.mute", {{"track", c.query()["tracks"][2]["id"]}, {"enabled", true}});
        run(c, "clip.trim", {{"clip", aID}, {"start_samples", 24000}, {"end_samples", 168000}});
        run(c, "clip.trim", {{"clip", bID}, {"start_samples", 36000}, {"end_samples", 156000}});
        run(c, "clip.fade",
            {{"clip", aID},
             {"in_samples", 2400},
             {"out_samples", 4800},
             {"in_curve", "linear"},
             {"out_curve", "linear"}});
        run(c, "clip.fade",
            {{"clip", bID},
             {"in_samples", 4800},
             {"out_samples", 2400},
             {"in_curve", "concave"},
             {"out_curve", "convex"}});
        run(c, "clip.gain", {{"clip", bID}, {"db", -6}});
        run(c, "group.create",
            {{"id", "linked"},
             {"name", "Linked edits"},
             {"members", Json::array({aTrack, bTrack})},
             {"enabled", true},
             {"edit", true},
             {"mute", false},
             {"solo", false}});
        pump();
        const auto before = c.query()["tracks"];
        auto baseline = render(c, dir, "original different source rates");
        auto trimPlan = c.makePlan(
            "human",
            Json::array({op("clip.trim", {{"clip", aID}, {"start_samples", 28800}, {"end_samples", 163200}})}));
        check(trimPlan["operations"].size() == 2 && trimPlan["operations"][1]["args"]["media_hash"] == bHash,
              "group trim expands actual peer with its own source hash");
        c.preview(trimPlan);
        check(c.query()["tracks"] == before, "preview never changes real Edit");
        auto receipt = c.commit(trimPlan), trimmed = c.query()["tracks"];
        check(clip(c, 0)["start_samples"] == 28800 && clip(c, 1)["start_samples"] == 40800 &&
                  clip(c, 0)["length_samples"] == 134400 && clip(c, 1)["length_samples"] == 110400 &&
                  clip(c, 0)["source_offset_samples"] == 28800 && clip(c, 1)["source_offset_samples"] == 40800 &&
                  trimmed[2] == before[2],
              "common edges retain different clip lengths source sync and outsider");
        auto trimmedPCM = render(c, dir, "trim both members");
        auto trimError = difference(trimmedPCM, baseline, 60000, 40000);
        check(trimError <= 2e-6, "group trim retains actual interior PCM phase at 48k and 44.1k");
        restore(c, receipt, before, trimmed);
        auto undoPCM = render(c, dir, "trim undone");
        check(difference(undoPCM, baseline, 0, 192000) <= 2e-6, "Undo restores complete native rendered PCM");

        auto fadeArgs = Json{{"clip", aID},
                             {"in_samples", 4800},
                             {"out_samples", 7200},
                             {"in_curve", "linear"},
                             {"out_curve", "linear"}};
        receipt = run(c, "clip.fade", fadeArgs);
        auto faded = c.query()["tracks"];
        check(clip(c, 1)["fade_in_samples"] == 7200 && clip(c, 1)["fade_out_samples"] == 4800 &&
                  clip(c, 1)["fade_in_curve"] == "concave" && clip(c, 1)["fade_out_curve"] == "convex" &&
                  faded[2] == before[2],
              "fade length delta retains peer curves and original length differences");
        auto fadedPCM = render(c, dir, "relative group fades");
        check(difference(fadedPCM, baseline, 60000, 40000) <= 2e-6 &&
                  difference(fadedPCM, baseline, 24000, 6000) > 1e-3,
              "native fades alter actual edge PCM and retain untouched interior");
        restore(c, receipt, before, faded);
        fadeArgs["in_curve"] = "s_curve";
        receipt = run(c, "clip.fade", fadeArgs);
        check(clip(c, 1)["fade_in_curve"] == "s_curve" && clip(c, 1)["fade_out_curve"] == "convex",
              "explicit curve change applies while unchanged other curve stays per member");
        c.undo(receipt["plan_id"]);
        receipt = run(c, "clip.gain", {{"clip", aID}, {"db", -3}});
        auto gained = c.query()["tracks"];
        check(std::abs(clip(c, 0)["gain_db"].get<double>() + 3) < 1e-5 &&
                  std::abs(clip(c, 1)["gain_db"].get<double>() + 9) < 1e-5,
              "relative group clip gain preserves six dB member difference");
        auto gainedPCM = render(c, dir, "relative group gain");
        auto gainError = difference(gainedPCM, baseline, 60000, 40000, std::pow(10., -3. / 20));
        check(gainError <= 2e-6, "actual group mix PCM follows -3 dB common gain");
        restore(c, receipt, before, gained);
        auto unchanged = c.query();
        fails(
            [&]
            {
                c.makePlan("human",
                           Json::array(
                               {op("clip.trim", {{"clip", aID}, {"start_samples", 144000}, {"end_samples", 168000}})}));
            },
            "trim making shorter peer empty is rejected atomically");
        fails(
            [&]
            {
                c.makePlan("human", Json::array({op("clip.fade", {{"clip", aID},
                                                                  {"in_samples", 0},
                                                                  {"out_samples", 0},
                                                                  {"in_curve", "linear"},
                                                                  {"out_curve", "linear"}})}));
            },
            "negative peer fade from relative change is refused");
        fails([&] { c.makePlan("human", Json::array({op("clip.gain", {{"clip", aID}, {"db", -100}})})); },
              "relative gain crossing peer SDK range is refused");
        fails(
            [&]
            {
                c.makePlan("human",
                           Json::array({op("clip.fade", fadeArgs), op("clip.fade", {{"clip", bID},
                                                                                    {"in_samples", 7201},
                                                                                    {"out_samples", 4800},
                                                                                    {"in_curve", "s_curve"},
                                                                                    {"out_curve", "convex"}})}));
            },
            "conflicting linked requests are rejected");
        fails(
            [&]
            {
                c.makePlan("human",
                           Json::array({op("clip.gain", {{"clip", aID}, {"db", -3}, {"media_hash", "stale"}})}));
            },
            "group planner retains supplied anchor evidence hash validation");
        check(c.query() == unchanged, "refused edits do not change state or history revision");
        auto stale = c.makePlan("human", Json::array({op("clip.fade", fadeArgs)}));
        receipt = run(c, "clip.lock", {{"clip", bID}, {"locked", true}});
        auto locked = c.query();
        fails([&] { c.makePlan("human", Json::array({op("clip.fade", fadeArgs)})); },
              "locked peer rejects whole group fade");
        fails(
            [&]
            {
                c.makePlan(
                    "human",
                    Json::array({op("clip.trim", {{"clip", aID}, {"start_samples", 28800}, {"end_samples", 168000}})}));
            },
            "locked peer rejects whole group trim");
        fails([&] { c.commit(stale); }, "stale plan cannot overwrite later human lock");
        check(c.query() == locked, "group refusal preserves locked peer and all members");
        c.undo(receipt["plan_id"]);
        pump();

        // Exercise production native pointer callbacks, not a mock timeline.
        c.updateUiState({{"edit_tool", "trim"}, {"edit_mode", "slip"}, {"span_samples", 192000}}, c.sessionToken());
        pump();
        auto* area = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(area, "native Edit timeline exists");
        auto r = area->clipRect(clip(c, 0), 0);
        auto from = juce::Point<float>{float(r.getX() + 1), float(r.getY() + 55)};
        auto to =
            from + juce::Point<float>{float(area->coordinates().pixelAt(4800) - area->coordinates().pixelAt(0)), 0};
        auto preDrag = c.query()["tracks"];
        area->mouseDown(event(*area, from));
        area->mouseDrag(event(*area, to, true));
        check(c.query()["tracks"] == preDrag, "linked trim gesture is only local draft until release");
        auto* peerHeader = find(w, "clip.select:" + text(bID));
        check(peerHeader, "peer clip header exists");
        auto previewX = peerHeader->getX();
        area->mouseUp(event(*area, to, true));
        pump();
        auto guiTrim = c.query()["tracks"];
        check(clip(c, 0)["start_samples"].get<int64_t>() > 24000 &&
                  clip(c, 1)["start_samples"].get<int64_t>() - 36000 ==
                      clip(c, 0)["start_samples"].get<int64_t>() - 24000,
              "real Trim pointer gesture commits equal member edge deltas");
        check(find(w, "clip.select:" + text(bID))->getX() == previewX,
              "peer preview geometry equals committed native geometry");
        c.undo();
        check(c.query()["tracks"] == preDrag, "one GUI Undo restores all group trims");
        c.redo();
        check(c.query()["tracks"] == guiTrim, "GUI Redo restores linked trim");
        c.undo();
        pump();

        // Right edge and Smart Tool fade handles use the same grouped draft and L1 writer.
        r = area->clipRect(clip(c, 0), 0);
        from = {float(r.getRight() - 1), float(r.getY() + 55)};
        to = from - juce::Point<float>{float(area->coordinates().pixelAt(4800) - area->coordinates().pixelAt(0)), 0};
        preDrag = c.query()["tracks"];
        area->mouseDown(event(*area, from));
        area->mouseDrag(event(*area, to, true));
        check(c.query()["tracks"] == preDrag, "right-edge group preview leaves Edit unchanged");
        area->mouseUp(event(*area, to, true));
        pump();
        check(clip(c, 0)["length_samples"].get<int64_t>() < 144000 &&
                  144000 - clip(c, 0)["length_samples"].get<int64_t>() ==
                      120000 - clip(c, 1)["length_samples"].get<int64_t>(),
              "real right-edge gesture trims members by identical amount");
        c.undo();
        check(c.query()["tracks"] == preDrag, "right-edge group edit undoes as one native transaction");
        c.updateUiState({{"edit_tool", "smart"}}, c.sessionToken());
        pump();
        for (const bool fadeIn : {true, false})
        {
            r = area->clipRect(clip(c, 0), 0);
            const auto key = fadeIn ? "fade_in_samples" : "fade_out_samples";
            const auto handle =
                fadeIn ? 24000 + clip(c, 0)[key].get<int64_t>() : 168000 - clip(c, 0)[key].get<int64_t>();
            from = {float(area->coordinates().pixelAt(handle)), float(r.getY() + 27)};
            const auto delta = float(area->coordinates().pixelAt(4800) - area->coordinates().pixelAt(0));
            to = from + juce::Point<float>{fadeIn ? delta : -delta, 0};
            preDrag = c.query()["tracks"];
            area->mouseDown(event(*area, from));
            area->mouseDrag(event(*area, to, true));
            check(c.query()["tracks"] == preDrag, "Smart fade handle remains a local group draft");
            area->mouseUp(event(*area, to, true));
            pump();
            check(clip(c, 0)[key].get<int64_t>() > preDrag[0]["clips"][0][key].get<int64_t>() &&
                      clip(c, 0)[key].get<int64_t>() - preDrag[0]["clips"][0][key].get<int64_t>() ==
                          clip(c, 1)[key].get<int64_t>() - preDrag[1]["clips"][0][key].get<int64_t>(),
                  "Smart fade pointer commits equal member length deltas");
            c.undo();
            check(c.query()["tracks"] == preDrag, "one native Undo restores both Smart fade handles");
            pump();
        }
        w.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        click(w, "clip.select:" + text(aID));
        check(w.uiCommands().getKeyMappings()->containsMapping(
                  280, juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0)),
              "native fade dialog has remappable Command F");
        check(w.uiCommands().invokeDirectly(280, false), "fade command opens real native panel");
        pump();
        auto* input = dynamic_cast<juce::TextEditor*>(find(w, "clip.fades.in_ms"));
        check(input, "fade input uses milliseconds");
        input->grabKeyboardFocus();
        check(juce::Component::getCurrentlyFocusedComponent() == input, "real native fade field owns keyboard focus");
        const auto textState = c.query();
        input->setText("123", false);
        input->selectAll();
        check(input->keyPressed(juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0)) &&
                  input->getHighlightedText() == "123" && c.query() == textState,
              "text Select All stays in the field instead of selecting project clips");
        input->keyPressed(juce::KeyPress('4', 0, '4'));
        check(input->getText() == "4" &&
                  input->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) &&
                  input->getText() == "123" && c.query() == textState,
              "text Undo reverses typing without undoing the audio project");
        input->setText("100", false);
        check(input->keyPressed(juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0)),
              "focused fade field routes Command Return through registered submit command");
        pump();
        check(!find(w, "clip.fades.panel") && clip(c, 0)["fade_in_samples"] == 4800 &&
                  clip(c, 1)["fade_in_samples"] == 7200,
              "actual native dialog commits one relative group fade");
        check(juce::Component::getCurrentlyFocusedComponent() == &w,
              "successful panel submission restores timeline command focus");
        auto dialogState = c.query()["tracks"];
        c.undo();
        check(c.query()["tracks"] == before, "fade dialog Undo restores all fields");
        c.redo();
        check(c.query()["tracks"] == dialogState, "fade dialog Redo preserves all member curves");
        c.undo();
        pump();
        w.uiCommands().invokeDirectly(280, false);
        pump();
        input = dynamic_cast<juce::TextEditor*>(find(w, "clip.fades.in_ms"));
        input->setText("invalid", false);
        unchanged = c.query();
        w.uiCommands().invokeDirectly(275, false);
        check(c.query() == unchanged && find(w, "clip.fades.panel"),
              "invalid duration retains dialog and changes nothing");
        check(input->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),
              "Escape from focused invalid field routes the registered cancel command");
        check(!find(w, "clip.fades.panel"), "Escape command cancels fade draft");
        check(juce::Component::getCurrentlyFocusedComponent() == &w,
              "panel cancellation restores timeline keyboard focus");
        const auto submitKey =
            juce::KeyPress('m', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        w.uiCommands().getKeyMappings()->removeKeyPress(submitKey);
        w.uiCommands().getKeyMappings()->clearAllKeyPresses(275);
        w.uiCommands().getKeyMappings()->addKeyPress(275, submitKey);
        pump();
        w.uiCommands().invokeDirectly(280, false);
        pump();
        input = dynamic_cast<juce::TextEditor*>(find(w, "clip.fades.in_ms"));
        input->setText("100", false);
        check(input->keyPressed(submitKey) && !find(w, "clip.fades.panel"),
              "focused panel uses remapped submit key rather than hardcoding Command Return");
        c.undo();
        w.uiCommands().getKeyMappings()->clearAllKeyPresses(275);
        w.uiCommands().getKeyMappings()->addKeyPress(
            275, juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0));
        pump();
        w.uiCommands().invokeDirectly(280, false);
        pump();
        receipt = run(c, "track.mute", {{"track", aTrack}, {"enabled", true}});
        unchanged = c.query();
        w.uiCommands().invokeDirectly(275, false);
        check(c.query() == unchanged && find(w, "clip.fades.panel"), "old fade dialog refuses later project edit");
        w.uiCommands().invokeDirectly(277, false);
        c.undo(receipt["plan_id"]);
        pump();

        auto key = juce::KeyPress('j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        w.uiCommands().getKeyMappings()->removeKeyPress(juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0));
        w.uiCommands().getKeyMappings()->removeKeyPress(key);
        w.uiCommands().getKeyMappings()->addKeyPress(280, key);
        check(w.uiCommands().getKeyMappings()->findCommandForKeyPress(key) == 280,
              "custom fade shortcut has one unambiguous command binding");
        pump();
        run(c, "clip.trim", {{"clip", aID}, {"start_samples", 28800}, {"end_samples", 163200}});
        run(c, "clip.fade", fadeArgs);
        pump();
        const auto saved = c.query()["tracks"];
        const auto project = dir.getChildFile("GroupTransforms.tracktionedit");
        c.save(project);
        Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("prefs-reopen")));
        reopened.setVisible(true);
        reopened.setSize(1600, 1000);
        reopened.openSession(project);
        pump();
        auto& opened = AudioDeviceTestAccess::owner(reopened);
        check(opened.query()["tracks"] == saved && opened.query()["mix_groups"] == c.query()["mix_groups"],
              "native save reopen retains group trim offsets fades gain stable IDs");
        check(reopened.uiCommands().getKeyMappings()->containsMapping(280, key),
              "custom fade shortcut survives reopen");
        check(reopened.uiCommands().getKeyMappings()->keyPressed(key, &reopened),
              "reopened custom shortcut dispatches actual native fade command");
        pump();
        check(find(reopened, "clip.fades.panel"), "custom shortcut visibly opens native fade controls");
        reopened.uiCommands().invokeDirectly(277, false);
        auto savedPCM = render(c, dir, "saved"), reopenPCM = render(opened, dir, "reopened");
        check(difference(savedPCM, reopenPCM, 0, 192000) <= 2e-6, "reopened native render retains complete PCM");

        // An owned file fixture supplies sub-sample source time; production still writes only via L1.
        auto xml = juce::XmlDocument::parse(project);
        check(bool(xml), "owned session XML decoded");
        bool found = false;
        std::function<void(juce::XmlElement&)> fractional = [&](juce::XmlElement& node)
        {
            if (node.hasTagName("AUDIOCLIP") && node.getStringAttribute("id") == text(bID))
            {
                node.setAttribute("offset", node.getDoubleAttribute("offset") + .25 / 48000.);
                found = true;
            }
            for (auto* child : node.getChildIterator())
                fractional(*child);
        };
        fractional(*xml);
        check(found, "actual audio clip source offset located");
        auto fractionalFile = dir.getChildFile("Fractional.tracktionedit");
        check(xml->writeTo(fractionalFile), "separate fractional fixture saved without original overwrite");
        opened.open(fractionalFile);
        const auto previous = clip(opened, 1)["source_offset_seconds"].get<double>();
        run(opened, "clip.trim", {{"clip", aID}, {"start_samples", 33600}, {"end_samples", 163200}});
        check(std::abs(clip(opened, 1)["source_offset_seconds"].get<double>() - previous - .1) <= 1e-12,
              "native group trim preserves fractional source time at 44.1k instead of rounding offset");
        check(Commands::mediaHash(aSource) == aHash && Commands::mediaHash(bSource) == bHash,
              "all edits and undo preserve both original media hashes");
        Json report{
            {"result", "passed"},
            {"checks", checks},
            {"test_directory", dir.getFullPathName().toStdString()},
            {"preview_session", project.getFullPathName().toStdString()},
            {"renders", measurements},
            {"trim_pcm_error", trimError},
            {"gain_pcm_error", gainError},
            {"budgets", {{"pcm_error", 2e-6}, {"fractional_offset_seconds", 1e-12}, {"render_ms", 10000}}},
            {"scope", "real native workspace callbacks/Edit/Undo/WAV; hardware GUI and listening not performed"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
