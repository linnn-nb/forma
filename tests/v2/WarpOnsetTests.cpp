// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectFixture.h"
#include "Workspace.h"
#include "ui/WorkspaceWindow.h"
#include "WarpOnsets.h"
#include <iostream>
#include <fstream>
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
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.refresh();
        w.selectAudioClip(id);
    }
    static std::string references(Commands& c)
    {
        return c.metadata.getChildWithName("ANALYSISARTIFACTS").toXmlString().toStdString();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
Json measurements = Json::array();
void check(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void wait(F f, const char* why, double ms = 5000)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + ms;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(f(), why);
}
template <class F> void rejects(F f, const char* why)
{
    bool bad = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        bad = true;
    }
    check(bad, why);
}
Json op(const char* name, Json a)
{
    return {{"command", name}, {"args", a}};
}
Json run(Commands& c, const char* name, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(name, a)})));
}
Json clip(Commands& c)
{
    return c.query()["tracks"][0]["clips"][0];
}
struct Storage : te::PropertyStorage
{
    juce::File folder;
    explicit Storage(juce::File f) : te::PropertyStorage("forma-onset-tests"), folder(f)
    {
        f.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return folder;
    }
};
void source(juce::File file, int sr)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(sr).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "known transient PCM24 source writer opens");
    juce::AudioBuffer<float> pcm(2, sr * 6);
    pcm.clear();
    for (double t : {.75, 1.5, 2.25, 3.5, 4.25, 5.25})
        for (int n = 0; n < int(sr * .1); ++n)
        {
            const auto frame = int(std::llround(t * sr)) + 37 + n;
            const float v =
                float(.4 * std::exp(-n / (sr * .035)) * std::cos(juce::MathConstants<double>::twoPi * 440 * n / sr));
            pcm.setSample(0, frame, v);
            pcm.setSample(1, frame, v * .5f);
        }
    check(writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "actual transient source PCM writes");
}
Json detect(Commands& c, const std::string& id)
{
    const auto before = c.querySummary();
    const auto history = c.query().at("history");
    const auto references = AudioDeviceTestAccess::references(c);
    const auto request = c.prepareWarpOnsets(id, Json::object(), c.sessionToken(), before.at("revision"));
    check(request.at("busy") && request.at("request").at("object_id") == id,
          "local draft has actual owned source request");
    wait([&] { return !c.analysisStatus().at("busy").get<bool>(); },
         "six second source analysis finishes inside fixed five seconds");
    const auto receipt = c.analysisStatus().at("receipt");
    check(receipt.at("state") == "completed" && receipt.at("current") == true,
          "real analysis has current completion receipt");
    check(c.querySummary() == before, "detection changes neither project revision nor native history");
    check(c.query().at("history") == history && AudioDeviceTestAccess::references(c) == references,
          "draft completion retains full previous history and analysis reference tree");
    return receipt;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root)
{
    wait([&] { return !c.playbackPreparation().at("busy").get<bool>(); },
         "actual SoundTouch preparation meets five second budget");
    const auto q = clip(c);
    const int64_t first = q.at("start_samples"), length = q.at("length_samples");
    const auto path = root.getChildFile("onset-render-" + juce::String(++serial) + ".wav");
    const auto r = c.render(path, first, first + length);
    check(r.at("audio_verified") && r.at("frames") == length && r.at("render_ms").get<double>() < 10000,
          "Tracktion renders actual Warp within fixed ten seconds");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(path), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == length,
          "actual output WAV decodes expected rate channels and duration");
    juce::AudioBuffer<float> pcm(2, int(length));
    check(reader->read(&pcm, 0, int(length), 0, true, true), "actual rendered PCM reads");
    return pcm;
}
juce::Component* find(juce::Component& c, const std::string& id)
{
    if (c.getComponentID().toStdString() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
} // namespace
int main(int argc, char** argv)
{
    const std::string output = argc > 1 ? argv[1] : "";
    auto saveReport = [&](const Json& value)
    {
        if (output.empty())
            return;
        std::ofstream stream(output);
        stream << value.dump(2);
        if (!stream.good())
            throw std::runtime_error("cannot write current report: " + output);
    };
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("Warp-Onsets-" + juce::Uuid().toString());
    root.createDirectory();
    saveReport(Json{{"result", "running"}});
    try
    {
        ndaw::desktop::WorkspaceWindow window(
            std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        auto& w = window.editor();
        window.showReady();
        auto& c = AudioDeviceTestAccess::commands(w);
        for (int sr : {48000, 44100})
        {
            if (sr == 48000)
                run(c, "track.create", {{"name", "Transient source"}, {"ref", "$t"}});
            else
                run(c, "clip.delete", {{"clip", clip(c).at("id")}});
            const auto media = root.getChildFile("transients-" + juce::String(sr) + ".wav");
            source(media, sr);
            const auto hash = Commands::mediaHash(media);
            const std::string tid = c.query()["tracks"][0]["id"];
            run(c, "clip.import",
                {{"track", tid}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
            const std::string id = clip(c).at("id");
            run(c, "clip.trim", {{"clip", id}, {"start_samples", 24000}, {"end_samples", 264000}});
            const auto original = clip(c);
            const auto bound = c.warpClip(id), receipt = detect(c, id);
            const auto suggestion = warp_onsets::candidates(bound, bound.at("anchors"), receipt);
            check(suggestion.at("items").size() == 6,
                  "actual detector finds six known interior energy rises with nonzero offset");
            size_t index = 0;
            double mappingError = 0;
            for (double t : {.75, 1.5, 2.25, 3.5, 4.25, 5.25})
            {
                t = (std::llround(t * sr) + 37) / double(sr);
                const auto& p = suggestion.at("items").at(index++);
                check(std::abs(p.at("source_frame").get<int64_t>() / double(sr) - t) <= .005,
                      "real onset source-frame error meets five millisecond budget");
                mappingError =
                    std::max(mappingError, std::abs(p.at("clip_time_samples").get<double>() - (t - .5) * 48000));
            }
            check(mappingError <= 1, "44.1/48 kHz source frames map to project samples within one sample");
            auto points = warp_onsets::insert(bound, bound.at("anchors"), suggestion.at("items"));
            check(points.size() == 8 && warp_onsets::insert(bound, points, suggestion.at("items")) == points,
                  "candidate adoption retains endpoints and skips duplicate source positions");
            points[2]["clip_time_samples"] = points[2]["clip_time_samples"].get<int64_t>() + 4800;
            Json args{{"clip", id},
                      {"enabled", true},
                      {"media_hash", hash},
                      {"anchors", points},
                      {"onset_artifact", receipt.at("artifact_id")}};
            auto invalid = args;
            invalid["onset_artifact"] = "invented";
            rejects([&] { run(c, "clip.warp", invalid); },
                    "invented evidence cannot become successful Warp provenance");
            const auto plan = c.makePlan("human", Json::array({op("clip.warp", args)}));
            const auto preview = c.preview(plan);
            check(clip(c) == original && preview.at("clip_changes")[0]["after"]["warp"]["onset_evidence"].size() == 1,
                  "preview includes accepted evidence without modifying Edit");
            const auto committed = c.commit(plan);
            const auto edited = clip(c);
            check(edited.at("warp").at("onset_evidence").size() == 1 && edited.at("warp").at("anchors") == points,
                  "one real L1 Warp transaction stores accepted source evidence and moved point");
            check(edited.at("warp").at("onset_evidence")[0].at("object_id") == id,
                  "accepted evidence retains actual analysed source object identity");
            c.undo(committed.at("plan_id"));
            check(clip(c) == original, "fresh Undo restores entire original geometry and evidence");
            c.redo();
            check(clip(c) == edited, "fresh Redo restores anchors and evidence");
            const auto pcm = render(c, root);
            int first = -1;
            double stereo = 0;
            // Locate the threshold within the declared window around the exact moved project sample.
            const double expectedOnset = points[2]["clip_time_samples"].get<double>() / 48000.;
            for (int n = int((expectedOnset - .08) * 48000); n < int((expectedOnset + .1) * 48000); ++n)
                if (first < 0 && std::abs(pcm.getSample(0, n)) > .05f)
                    first = n;
            for (int n = 0; n < pcm.getNumSamples(); ++n)
                stereo = std::max(stereo, std::abs(double(pcm.getSample(1, n) - .5f * pcm.getSample(0, n))));
            const double timingError = first / 48000. - expectedOnset;
            check(first >= 0 && std::abs(timingError) <= .05,
                  "actual SoundTouch onset follows moved detected anchor within fifty milliseconds");
            check(stereo <= 2e-7, "actual rendered stereo relationship meets fixed tolerance");
            const auto saved = root.getChildFile("Transient-Warp-" + juce::String(sr) + ".tracktionedit");
            c.save(saved);
            w.openSession(saved);
            check(clip(c) == edited, "saved project reopens with exact onset records and anchor identities");
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("cold-" + juce::String(sr))));
            cold.open(saved);
            check(clip(cold) == edited, "independent fresh Engine loads actual anchors and source evidence");
            const auto again = render(cold, root);
            double error = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < pcm.getNumSamples(); ++n)
                    error = std::max(error, std::abs(double(pcm.getSample(ch, n) - again.getSample(ch, n))));
            check(error <= 2e-7, "cold reopened actual Warp audio matches full output within fixed tolerance");
            cold.undo();
            check(clip(cold) == original, "independent Engine original Undo restores pre-detection project");
            cold.redo();
            check(clip(cold) == edited, "independent Engine original Redo restores accepted measured Warp");
            run(cold, "clip.warp", {{"clip", id}, {"enabled", false}});
            check(!clip(cold).at("warp").at("enabled").get<bool>() &&
                      clip(cold).at("warp").at("onset_evidence") == edited.at("warp").at("onset_evidence"),
                  "disabling Warp retains historical source measurements");
            run(cold, "clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", points}});
            check(clip(cold) == edited, "reenabling manual Warp preserves accepted provenance without reanalysis");
            cold.undo();
            cold.undo();
            check(clip(cold) == edited, "Undo reenable and disable restores original accepted Warp transaction");
            c.undo();
            check(clip(c) == original, "original persistent Undo removes measured Warp as one transaction");
            if (sr == 48000)
            {
                const auto redoFile = root.getChildFile("Onsets-Redo-Only.tracktionedit");
                c.save(redoFile);
                auto envelope = juce::XmlDocument::parse(redoFile);
                auto* feature =
                    envelope->getChildByName("REQUIRES")->getChildByAttribute("id", "audio.warp.onset-evidence");
                check(feature && feature->getIntAttribute("version") == 1,
                      "Redo-only measured Warp retains explicit old-reader protection");
                w.openSession(redoFile);
                check(clip(c) == original, "Redo-only measured Warp reopens without inventing active evidence");
            }
            c.redo();
            check(clip(c) == edited, "original persistent Redo restores measured Warp");
            auto stale = args;
            stale["onset_artifact"] = detect(c, id).at("artifact_id");
            run(c, "track.gain", {{"track", tid}, {"db", sr == 48000 ? -1. : -2.}});
            rejects([&] { run(c, "clip.warp", stale); }, "old revision evidence cannot overwrite later human edits");
            if (sr == 48000)
            {
                auto native = juce::ValueTree::fromXml(*project_fixture::nativeXml(saved));
                auto meta = native.getChildWithName("NATIVEDAW");
                meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
                auto damage = [&](auto&& self, juce::ValueTree n) -> bool
                {
                    if (n.hasType("AUDIOCLIP"))
                        n.setProperty("warpTime", false, nullptr);
                    if (n.hasType("NDAW_WARP_ONSETS"))
                    {
                        n.setProperty("sha256", "damaged", nullptr);
                        return true;
                    }
                    for (auto child : n)
                        if (self(self, child))
                            return true;
                    return false;
                };
                check(damage(damage, native), "owned historyless corruption fixture changes real onset evidence");
                const auto invalidFile = root.getChildFile("Damaged-Onsets.tracktionedit");
                check(invalidFile.replaceWithText(native.toXmlString()), "corrupted fixture is a real input file");
                const auto beforeBadOpen = c.query();
                const auto session = c.sessionToken();
                bool refused = false;
                try
                {
                    c.open(invalidFile);
                }
                catch (const std::exception& e)
                {
                    refused = std::string(e.what()).find("Warp瞬态") != std::string::npos;
                }
                check(refused, "damaged source evidence specifically rejects even disabled Warp before adopting Edit");
                check(c.query().at("tracks") == beforeBadOpen.at("tracks") &&
                          c.query().at("history") == beforeBadOpen.at("history") && c.sessionToken() == session,
                      "failed evidence load preserves current tracks history and session");
            }
            check(Commands::mediaHash(media) == hash, "original source bytes remain unchanged");
            measurements.push_back({{"source_sample_rate", sr},
                                    {"source_hash", hash},
                                    {"receipt", receipt},
                                    {"mapping_error_samples", mappingError},
                                    {"expected_onset_seconds", expectedOnset},
                                    {"onset_error_seconds", timingError},
                                    {"stereo_error", stereo},
                                    {"reopen_pcm_error", error},
                                    {"project", saved.getFullPathName().toStdString()}});
        }
        // The same real source measurement and L1 transaction through native controls and remapped keys.
        c.undo(); // track gain
        c.undo(); // Warp
        const auto original = clip(c);
        const std::string id = original.at("id");
        AudioDeviceTestAccess::select(w, id);
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress analyseKey(juce::KeyPress::F15Key,
                                        juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier, 0),
            adoptKey(juce::KeyPress::F16Key, juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier, 0);
        check(keys->findCommandForKeyPress(analyseKey) == 0 && keys->findCommandForKeyPress(adoptKey) == 0,
              "custom test bindings are unassigned before remap");
        keys->addKeyPress(650, analyseKey);
        keys->addKeyPress(651, adoptKey);
        wait([&] { return w.queryView().value("keymap_xml", std::string{}).find("F15") != std::string::npos; },
             "custom onset keymap persists into session UI state", 2000);
        check(keys->findCommandForKeyPress(analyseKey) == 650 && keys->findCommandForKeyPress(adoptKey) == 651,
              "actual mappings resolve to intended onset commands");
        check(w.uiCommands().invokeDirectly(573, false), "registered command opens actual native Warp panel");
        auto* panel = find(w, "clip.warp.panel");
        auto* profile = dynamic_cast<juce::ComboBox*>(find(w, "clip.warp.onset_profile"));
        check(panel && panel->isVisible() && profile, "actual Warp panel exposes detector profile");
        window.toFront(true);
        profile->grabKeyboardFocus();
        wait([&] { return window.getPeer()->isFocused() && profile->hasKeyboardFocus(false); },
             "native peer and profile own focus before custom analyse key", 2000);
        check(window.getPeer()->handleKeyPress(analyseKey), "actual native peer routes remapped analysis shortcut");
        wait([&] { return find(w, "clip.warp.onset:5") != nullptr; },
             "GUI obtains six actual transient candidates within five seconds");
        auto* firstBox = dynamic_cast<juce::ToggleButton*>(find(w, "clip.warp.onset:0"));
        check(firstBox && firstBox->getToggleState(), "returned energy candidate is selectable native checkbox");
        firstBox->triggerClick();
        wait([&] { return !firstBox->getToggleState(); }, "actual checkbox deselects one candidate", 2000);
        firstBox->grabKeyboardFocus();
        wait([&] { return firstBox->hasKeyboardFocus(false) && window.getPeer()->isFocused(); },
             "native candidate owns actual focus", 2000);
        check(window.getPeer()->handleKeyPress(adoptKey), "native candidate routes remapped adoption shortcut");
        auto* choice = dynamic_cast<juce::ComboBox*>(find(w, "clip.warp.anchors"));
        wait([&] { return choice && choice->getNumItems() == 7; },
             "five selected candidates join actual draft with two endpoints", 2000);
        check(clip(c) == original, "GUI candidate detection and adoption do not bypass L1");
        choice->setSelectedId(2, juce::sendNotificationSync);
        auto* out = dynamic_cast<juce::TextEditor*>(find(w, "clip.warp.position_ms"));
        out->setText("1100", false);
        auto* set = dynamic_cast<juce::Button*>(find(w, "clip.warp.set_point"));
        set->triggerClick();
        wait([&] { return choice->getItemText(1).contains("1100.000"); },
             "native numeric editor moves detected source point", 2000);
        {
            juce::PNGImageFormat png;
            juce::FileOutputStream image(root.getChildFile("Warp-Onsets-Native-Panel.png"));
            const auto snapshot =
                panel->createComponentSnapshot(panel->getLocalBounds(), true, 1.f, juce::SoftwareImageType{});
            const int top = (panel->getHeight() - std::min(820, panel->getHeight() - 20)) / 2;
            check(snapshot.getPixelAt(panel->getWidth() / 2, top + 12) == ndaw::desktop::base(),
                  "native component capture includes actual opaque card painting, not partial native-image backing");
            check(image.openedOk() && png.writeImageToStream(snapshot, image),
                  "actual native component snapshot records current measured draft");
        }
        out->grabKeyboardFocus();
        wait([&] { return out->hasKeyboardFocus(false) && window.getPeer()->isFocused(); },
             "actual editable field owns focus before commit", 2000);
        const auto accept = keys->getKeyPressesAssignedToCommand(275)[0];
        check(keys->findCommandForKeyPress(accept) == 275 && window.getPeer()->handleKeyPress(accept),
              "native accept key submits same L1 Warp transaction");
        wait([&] { return !panel->isVisible() && clip(c).at("warp").at("onset_evidence").size() == 1; },
             "GUI displays completion only after real commit receipt", 2000);
        const auto guiEdited = clip(c);
        check(guiEdited.at("warp").at("anchors")[1].at("source_time_seconds").get<double>() ==
                  (1.5 * 44100 + 37) / 44100.,
              "moving only target time preserves detected source double exactly despite formatted numeric field");
        render(c, root);
        const auto guiFile = root.getChildFile("Transient-Warp-GUI.tracktionedit");
        c.save(guiFile);
        w.openSession(guiFile);
        check(clip(c) == guiEdited && keys->findCommandForKeyPress(analyseKey) == 650 &&
                  keys->findCommandForKeyPress(adoptKey) == 651,
              "GUI accepted geometry evidence and custom bindings survive real reopen");
        c.undo();
        check(clip(c) == original, "GUI measured Warp is one original persistent Undo");
        c.redo();
        check(clip(c) == guiEdited, "GUI measured Warp is one original persistent Redo");
        // A detached malformed provenance record must also fail while Warp is disabled.
        auto record = juce::ValueTree("NDAW_WARP_ONSETS");
        record.setProperty("schema", 2, nullptr);
        record.setProperty("record", "[]", nullptr);
        record.setProperty("sha256", "bad", nullptr);
        rejects([&] { warp_onsets::read(record, "bad"); },
                "unsupported onset record schema is never accepted as measured data");
        const Json result{{"result", "passed"},
                          {"checks", checks},
                          {"root", root.getFullPathName().toStdString()},
                          {"measurements", measurements}};
        saveReport(result);
        return 0;
    }
    catch (const std::exception& e)
    {
        const Json result{{"result", "failed"},
                          {"checks", checks},
                          {"error", e.what()},
                          {"root", root.getFullPathName().toStdString()}};
        saveReport(result);
        std::cerr << result.dump(2) << std::endl;
        return 1;
    }
}
