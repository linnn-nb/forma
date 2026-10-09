#include "Workspace.h"
#include "TimelineState.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using ndaw::desktop::Workspace;
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
void check(bool ok, const char* description)
{
    if (!ok)
        throw std::runtime_error(description);
    ++checks;
    std::cout << "PASS " << description << std::endl;
}
template <class F> void rejects(F f, const char* description)
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
    check(failed, description);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
}
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
Json run(Commands& c, Json ops)
{
    auto r = c.commit(c.makePlan("human", std::move(ops)));
    pump();
    return r;
}
Json togglePlan(Commands& c, bool enabled, const std::string& actor = "human")
{
    return c.makePlan(actor, Json::array({op("session.automation_follows_edit.set", {{"enabled", enabled}})}));
}
Json lanes(Commands& c, const std::string& track)
{
    auto result = c.automationQuery(track)["lanes"];
    for (auto& lane : result)
        for (const char* key : {"value", "explicit_value", "display", "recording"})
            lane.erase(key);
    return result;
}
bool hasFollow(const Json& plan)
{
    for (const auto& operation : plan["operations"])
        if (operation["command"] == "automation.range.shuffle" || operation["command"] == "automation.range.paste")
            return true;
    return false;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& file, int64_t end)
{
    const auto receipt = c.render(file, 0, end);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    check(reader && reader->numChannels == 2 && reader->sampleRate == 48000 && reader->lengthInSamples == end &&
              receipt["frames"] == end,
          "real native render receipt matches independently decoded stereo WAV");
    juce::AudioBuffer<float> pcm(2, int(end));
    check(reader->read(&pcm, 0, int(end), 0, true, true), "actual rendered PCM decoded");
    return pcm;
}
double compare(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int end)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < end; ++i)
        {
            // Existing default WaveNode split-boundary qualification: fixed budget, fixed exclusion.
            if (std::abs(i - 48000) <= 2048 || std::abs(i - 96000) <= 2048)
                continue;
            error = std::max(error, std::abs(double(a.getSample(ch, i)) - b.getSample(ch, i)));
        }
    check(error <= 2e-5, "audio-only time collapse/paste retains absolute-time automation in real PCM (2e-5)");
    return error;
}
juce::XmlElement* options(juce::XmlElement& node)
{
    if (node.hasTagName("EDIT_OPTIONS"))
        return &node;
    for (auto* child = node.getFirstChildElement(); child; child = child->getNextElement())
        if (auto* found = options(*child))
            return found;
    return nullptr;
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File p) : PropertyStorage("Forma automation follow tests"), path(p) {}
    juce::File getAppPrefsFolder() override
    {
        path.createDirectory();
        return path;
    }

private:
    juce::File path;
};
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                           : juce::File::getSpecialLocation(juce::File::tempDirectory);
    auto dir = parent.getChildFile("automation-follow-" + juce::Uuid().toString());
    dir.createDirectory();
    double maximumPcm = 0;
    const bool withDevice = argc > 3 && std::string(argv[3]) == "--device";
    try
    {
        Commands c(false);
        check(c.editingOptions() == Json({{"schema", 1}, {"automation_follows_edit", true}}),
              "legacy missing options defaults to previously qualified follow-on behaviour");
        check(c.query()["editing_options"] == c.querySummary()["editing_options"],
              "full and summary facts expose the same persisted option");
        const auto legacy = dir.getChildFile("LegacyNoOptions.tracktionedit");
        c.save(legacy);
        c.open(legacy);
        check(c.editingOptions()["automation_follows_edit"] && !c.query()["can_undo"].get<bool>(),
              "opening a legacy Edit defaults on without inventing a transaction");
        const auto media = dir.getChildFile("ActualStereo.wav");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = format.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        check(bool(writer), "real PCM source opened");
        juce::AudioBuffer<float> source(2, 240000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < source.getNumSamples(); ++i)
                source.setSample(
                    ch, i, float(.05 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * i / 48000.)));
        check(writer->writeFromAudioSampleBuffer(source, 0, source.getNumSamples()), "real stereo PCM written");
        writer.reset();
        const auto originalHash = Commands::mediaHash(media);
        run(c, Json::array(
                   {op("track.create", {{"name", "Follow edit diagnostic"}, {"ref", "$s"}}),
                    op("track.create", {{"name", "Destination without EQ"}, {"ref", "$d"}}),
                    op("clip.import",
                       {{"track", "$s"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}}),
                    op("plugin.insert", {{"track", "$s"}, {"type", "4bandEq"}})}));
        const auto facts = c.query();
        const std::string track = facts["tracks"][0]["id"], target = facts["tracks"][1]["id"],
                          clip = facts["tracks"][0]["clips"][0]["id"];
        std::string eq;
        for (const auto& plugin : facts["tracks"][0]["plugins"])
            if (plugin["type"] == "4bandEq")
                eq = plugin["id"].get<std::string>() + "::Mid gain 1";
        check(!eq.empty(), "native EQ parameter is actually enumerated");
        Json points = Json::array();
        for (const auto& parameter : {std::string("volume"), std::string("pan"), eq})
            for (int n = 0; n < 4; ++n)
                points.push_back(op("automation.point.add", {{"track", track},
                                                             {"parameter", parameter},
                                                             {"position_samples", int64_t(n * 80000)},
                                                             {"value", parameter == "volume" ? -18. + n * 3
                                                                       : parameter == "pan"  ? -.4 + n * .2
                                                                                             : 0.},
                                                             {"curve", .5},
                                                             {"ref", "$p" + std::to_string(points.size())}}));
        run(c, points);
        const auto originals = lanes(c, track), originalClips = c.query()["tracks"][0]["clips"];
        const auto baseline = render(c, dir.getChildFile("Original.wav"), 240000);
        auto copied =
            c.prepareClipboard(Json::array({{{"clip", clip}, {"start_samples", 48000}, {"end_samples", 96000}}}),
                               Json::array({track}), 48000, 96000, c.sessionToken(), c.query()["revision"]);
        const std::string frozenOn = copied["id"];
        c.acceptClipboard(frozenOn);
        check(copied["automation_follows_edit"] && copied["automation"].size() == 3,
              "copy-on freezes actual volume pan EQ curves with the captured option");
        const auto oldShuffle = c.makeShuffleRangePlan(Json::array({track}), 48000, 96000);
        check(hasFollow(oldShuffle), "on range compiler includes real automation follow operations");
        const auto toggle = togglePlan(c, false);
        const auto preview = c.preview(toggle);
        check(preview["editing_option_changes"].size() == 1 && preview["time_selection_changes"].empty() &&
                  preview["editing_option_changes"][0]["before"] == true &&
                  preview["editing_option_changes"][0]["after"] == false,
              "option preview shows typed before/after without falsely reporting a time selection");
        const auto receipt = c.commit(toggle);
        check(!c.editingOptions()["automation_follows_edit"].get<bool>() &&
                  c.query()["revision"].get<uint64_t>() == toggle["base_revision"].get<uint64_t>() + 1,
              "option commits through one native transaction and advances revision");
        const auto replay = c.commit(toggle);
        check(replay["replayed"] == true && replay["plan_id"] == receipt["plan_id"] &&
                  replay["revision"] == receipt["revision"] && replay["state"] == "committed",
              "duplicate option submission replays original receipt without a second transaction");
        rejects([&] { c.commit(oldShuffle); }, "planning under prior option cannot overwrite changed revision");
        auto forged = oldShuffle;
        forged["base_revision"] = c.query()["revision"];
        rejects([&] { c.preview(forged); }, "rewriting revision cannot bypass complete range recompilation");
        c.undo();
        pump();
        check(c.editingOptions()["automation_follows_edit"] == true && lanes(c, track) == originals,
              "native Undo restores default-on and preserves all real curves");
        c.redo();
        pump();
        check(c.editingOptions()["automation_follows_edit"] == false, "native Redo restores off");
        const auto beforeRefusals = c.query();
        rejects(
            [&]
            {
                c.makePlan("agent:external",
                           Json::array({op("session.automation_follows_edit.set", {{"enabled", true}})}));
            },
            "external Agent cannot silently change human editing policy");
        rejects([&] { togglePlan(c, true, "extension:untrusted"); }, "extension cannot change human editing policy");
        rejects([&] { togglePlan(c, false); }, "no-op option is rejected without an empty Undo transaction");
        rejects(
            [&]
            {
                c.makePlan("human", Json::array({op("session.automation_follows_edit.set", {{"enabled", true}}),
                                                 op("track.mute", {{"track", track}, {"muted", true}})}));
            },
            "option change cannot mix with edits planned against its previous value");
        rejects([&]
                { c.makePlan("human", Json::array({op("session.automation_follows_edit.set", {{"enabled", 1}})})); },
                "command schema rejects integer pretending to be boolean");
        check(c.query() == beforeRefusals, "all invalid policy requests preserve complete facts and revision");
        const auto cut = c.makeShuffleRangePlan(Json::array({track}), 48000, 96000);
        check(!hasFollow(cut), "off range compiler does not inspect or edit automation");
        c.commit(cut);
        pump();
        const auto cutClips = c.query()["tracks"][0]["clips"];
        check(lanes(c, track) == originals && cutClips.size() == 2 && cutClips[1]["start_samples"] == 48000,
              "off Shuffle removes real audio time while every native curve and point identity stays put");
        maximumPcm = compare(baseline, render(c, dir.getChildFile("OffCut.wav"), 192000), 192000);
        c.undo();
        pump();
        check(c.query()["tracks"][0]["clips"] == originalClips && lanes(c, track) == originals,
              "audio-only Cut Undo restores original native clip and curves");
        c.redo();
        pump();
        check(c.query()["tracks"][0]["clips"] == cutClips && lanes(c, track) == originals,
              "audio-only Cut Redo restores exact clip IDs without curve edits");
        const auto paste = c.makeClipboardPastePlan(frozenOn, Json::array({track}), 48000, 48000, "shuffle");
        check(!hasFollow(paste), "current off setting takes precedence over a previously copied-on snapshot");
        c.commit(paste);
        pump();
        const auto roundtrip = c.query()["tracks"][0]["clips"];
        check(lanes(c, track) == originals && roundtrip.size() == 3,
              "off paste restores real audio without pasting or shifting frozen curves");
        maximumPcm =
            std::max(maximumPcm, compare(baseline, render(c, dir.getChildFile("OffRoundtrip.wav"), 240000), 240000));
        c.undo();
        pump();
        c.redo();
        pump();
        check(c.query()["tracks"][0]["clips"] == roundtrip && lanes(c, track) == originals,
              "off paste Undo/Redo restores exact native audio and untouched curves");
        const auto demo = dir.getChildFile("AutomationFollowDemo.tracktionedit");
        c.save(demo);
        c.open(demo);
        pump();
        check(c.editingOptions()["automation_follows_edit"] == false && lanes(c, track) == originals &&
                  c.query()["tracks"][0]["clips"] == roundtrip,
              "actual save/close/reopen retains off policy, audio and all point identities");
        auto xml = juce::XmlDocument::parse(demo);
        check(xml && options(*xml) && options(*xml)->getStringAttribute("automation_follows_edit") == "0",
              "canonical persisted Edit XML contains the engineering option");
        for (int variant = 0; variant < 9; ++variant)
        {
            auto broken = std::make_unique<juce::XmlElement>(*xml);
            auto* node = options(*broken);
            if (variant == 0)
                node->setAttribute("schema", 2);
            if (variant == 1)
                node->setAttribute("automation_follows_edit", 2);
            if (variant == 2)
                node->setAttribute("automation_follows_edit", "true");
            if (variant == 3)
                node->setAttribute("automation_follows_edit", "1.5");
            if (variant == 4)
                node->removeAttribute("automation_follows_edit");
            if (variant == 5)
                node->setAttribute("extra", "ignored?");
            if (variant == 6)
                node->createNewChildElement("unexpected");
            if (variant == 7)
                broken->getChildByName("NATIVEDAW")->addChildElement(new juce::XmlElement(*node));
            if (variant == 8)
                node->setAttribute("automation_follows_edit", "9999999999999999999999");
            const auto bad = dir.getChildFile("BadOptions.tracktionedit");
            broken->writeTo(bad);
            const auto before = c.query();
            rejects([&] { c.open(bad); }, "corrupt/duplicate option rejected before adopting candidate Edit");
            check(c.query() == before, "failed option adoption preserves session token, revision and native objects");
        }
        Json ids = Json::array();
        for (const auto& item : roundtrip)
            if (item["start_samples"].get<int64_t>() < 96000 &&
                item["start_samples"].get<int64_t>() + item["length_samples"].get<int64_t>() > 48000)
                ids.push_back({{"clip", item["id"]},
                               {"start_samples", std::max(int64_t(48000), item["start_samples"].get<int64_t>())},
                               {"end_samples", std::min(int64_t(96000), item["start_samples"].get<int64_t>() +
                                                                            item["length_samples"].get<int64_t>())}});
        copied = c.prepareClipboard(ids, Json::array({track}), 48000, 96000, c.sessionToken(), c.query()["revision"]);
        const std::string frozenOff = copied["id"];
        c.acceptClipboard(frozenOff);
        check(copied["automation_follows_edit"] == false && copied["automation"].empty(),
              "copy-off captures audio without fabricating hidden curve snapshots");
        c.commit(togglePlan(c, true));
        pump();
        const auto copyOffPaste = c.makeClipboardPastePlan(frozenOff, Json::array({target}), 48000, 48000, "shuffle");
        c.commit(copyOffPaste);
        pump();
        check(lanes(c, target)[0]["points"].empty() && c.query()["tracks"][1]["clips"].size() == 1,
              "later on paste cannot invent source automation that was not copied");
        c.undo();
        pump();
        c.undo();
        pump();
        copied = c.prepareClipboard(ids, Json::array({track}), 48000, 96000, c.sessionToken(), c.query()["revision"]);
        c.commit(togglePlan(c, true));
        pump();
        auto onSnapshot =
            c.prepareClipboard(ids, Json::array({track}), 48000, 96000, c.sessionToken(), c.query()["revision"]);
        c.acceptClipboard(onSnapshot["id"]);
        rejects([&] { c.makeClipboardPastePlan(onSnapshot["id"], Json::array({target}), 48000, 48000, "shuffle"); },
                "on paste refuses genuinely missing source EQ parameter mapping");
        c.commit(togglePlan(c, false));
        pump();
        const auto foreignOff =
            c.makeClipboardPastePlan(onSnapshot["id"], Json::array({target}), 48000, 48000, "shuffle");
        check(!hasFollow(foreignOff), "off paste does not impose unrelated automation plugin compatibility");
        c.commit(foreignOff);
        pump();
        c.undo();
        pump();
        check(c.query()["tracks"][1]["clips"].empty() && lanes(c, track) == originals,
              "foreign audio-only paste and Undo preserve all source facts");
        {
            Workspace w(withDevice, std::make_unique<Storage>(dir.getChildFile("GuiPrefs")));
            w.setVisible(true);
            w.setSize(1440, 1000);
            w.openSession(demo);
            pump();
            auto& owner = AudioDeviceTestAccess::owner(w);
            auto* button = dynamic_cast<juce::Button*>(find(w, "editing.automation_follows_edit"));
            check(button && button->isVisible() && button->isEnabled() && !button->getToggleState(),
                  "production toolbar shows the actual reopened off option");
            auto info = w.uiCommands().getCommandForID(283);
            check(info && !(info->flags & juce::ApplicationCommandInfo::isTicked),
                  "native menu checkbox follows actual off fact");
            const auto originalRevision = owner.query()["revision"];
            const juce::KeyPress defaultKey('a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
            check(w.keyPressed(defaultKey), "native default Control Option A shortcut executes");
            pump();
            check(owner.editingOptions()["automation_follows_edit"] && button->getToggleState() &&
                      owner.query()["revision"].get<uint64_t>() == originalRevision.get<uint64_t>() + 1,
                  "shortcut and native toolbar share one L1 engineering transaction");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(!button->getToggleState() && owner.editingOptions()["automation_follows_edit"] == false,
                  "GUI Undo restores policy and visible indicator");
            w.uiCommands().invokeDirectly(7, false);
            pump();
            check(button->getToggleState(), "GUI Redo restores actual policy indicator");
            auto* keys = w.uiCommands().getKeyMappings();
            keys->clearAllKeyPresses(283);
            const juce::KeyPress custom('a',
                                        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier |
                                            juce::ModifierKeys::shiftModifier,
                                        0);
            keys->addKeyPress(283, custom);
            pump();
            const auto guiSaved = dir.getChildFile("GuiSaved.tracktionedit");
            owner.save(guiSaved);
            w.openSession(demo);
            pump();
            w.openSession(guiSaved);
            pump();
            check(keys->containsMapping(283, custom) && !keys->containsMapping(283, defaultKey),
                  "actual native custom shortcut survives other-session switch and reopening");
            check(w.keyPressed(custom), "reopened customized shortcut executes");
            pump();
            check(!button->getToggleState() && owner.editingOptions()["automation_follows_edit"] == false,
                  "custom shortcut edits current engineering option rather than stale UI state");
            button->triggerClick();
            pump();
            check(button->getToggleState(), "real toolbar button uses same command and native Undo path");
            if (withDevice)
            {
                owner.play();
                pump();
                juce::ApplicationCommandInfo inactive(283);
                w.getCommandInfo(283, inactive);
                check(inactive.flags & juce::ApplicationCommandInfo::isDisabled,
                      "policy change disabled during real native playback");
                rejects([&] { togglePlan(owner, false); }, "L1 also refuses policy edit during playback");
                owner.stop();
                pump();
            }
            w.setSize(1100, 880);
            pump();
            check(!button->isVisible(), "narrow window hides indicator without overlapping transport controls");
            juce::ApplicationCommandInfo menuOnly(283);
            w.getCommandInfo(283, menuOnly);
            check(!(menuOnly.flags & juce::ApplicationCommandInfo::isDisabled),
                  "menu and shortcut remain usable in narrow window");
        }
        check(Commands::mediaHash(media) == originalHash,
              "all copy/edit/save/undo operations preserve source PCM hash");
        Json result{{"result", "passed"},
                    {"test", "U-P0-AUTOMATION-FOLLOW-01"},
                    {"checks", checks},
                    {"max_pcm_error", maximumPcm},
                    {"device_playback_checks", withDevice ? "executed" : "not_executed"},
                    {"pcm_budget", 2e-5},
                    {"split_exclusion_samples", 2048},
                    {"demo", demo.getFullPathName().toStdString()},
                    {"scope", "actual native Edit, Undo, curves, renderer and production JUCE widgets; "
                              "desktop/hardware/listening separately qualified"}};
        if (argc > 1)
        {
            std::ofstream output(argv[1]);
            output << result.dump(2);
            if (!output)
                throw std::runtime_error("report write failed");
        }
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
