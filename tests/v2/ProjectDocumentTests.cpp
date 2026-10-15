// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectDocument.h"
#include "NativeEditXml.h"
#include "RecoveryStore.h"
#include "Workspace.h"
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
    static std::pair<juce::ValueTree, Json> snapshot(Commands& c)
    {
        return c.recoverySnapshot();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
double pcmError = 0;
Json rejected = Json::array();
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
struct Storage : te::PropertyStorage
{
    juce::File dir;
    explicit Storage(juce::File f) : te::PropertyStorage("Forma project document tests"), dir(f) {}
    juce::File getAppPrefsFolder() override
    {
        dir.createDirectory();
        return dir;
    }
};
Json run(Commands& c, const char* command, Json args)
{
    auto receipt = c.commit(c.makePlan("human", Json::array({{{"command", command}, {"args", args}}})));
    check(receipt["state"] == "committed", "real L1 edit committed");
    return receipt;
}
juce::File source(juce::File dir)
{
    const auto file = dir.getChildFile("Original.wav");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "real PCM24 stereo source writer opens");
    juce::AudioBuffer<float> samples(2, 48000);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 48000; ++i)
            samples.setSample(
                ch, i, .15f * float(std::sin(juce::MathConstants<double>::twoPi * (ch ? 997 : 440) * i / 48000.)));
    check(writer->writeFromAudioSampleBuffer(samples, 0, 48000), "actual source frames write");
    return file;
}
juce::AudioBuffer<float> render(Commands& c, juce::File dir)
{
    const auto until = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (c.playbackPreparation()["busy"].get<bool>() && juce::Time::getMillisecondCounterHiRes() < until)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    check(!c.playbackPreparation()["busy"].get<bool>(), "actual processing source ready within fixed 5s");
    const auto file = dir.getChildFile("Render-" + juce::String(++serial) + ".wav");
    const auto receipt = c.render(file, 0, 48000);
    check(receipt["audio_verified"] && receipt["render_ms"].get<double>() < 10000, "real WAV receipt within fixed 10s");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->bitsPerSample == 24 &&
              reader->lengthInSamples == 48000,
          "independent decoder verifies exact real WAV format and frames");
    juce::AudioBuffer<float> pcm(2, 48000);
    check(reader->read(&pcm, 0, 48000, 0, true, true), "actual rendered PCM decodes");
    return pcm;
}
void compare(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 48000; ++i)
            pcmError = std::max(pcmError, std::abs(double(a.getSample(ch, i)) - b.getSample(ch, i)));
    check(pcmError <= 2e-6, "full real WAV survives save, cold reopen and original Redo within fixed tolerance");
}
void refuse(Commands& c, const juce::XmlElement& xml, juce::File dir, const std::string& name)
{
    const auto path = dir.getChildFile(juce::String(name) + ".tracktionedit");
    check(xml.writeTo(path), "actual incompatible document writes in owned fixture");
    const auto before = c.query()["tracks"];
    const auto token = c.sessionToken();
    std::string error;
    try
    {
        c.open(path);
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }
    check(!error.empty(), "incompatible or damaged document is rejected by real L1 open");
    const std::map<std::string, std::string> expected{{"Unknown-Capability", "future.required.engine"},
                                                      {"Future-Envelope", "封装版本"},
                                                      {"Future-Capability-Version", "v999"},
                                                      {"Missing-Requirements", "能力声明"},
                                                      {"Duplicate-Capability", "重复"},
                                                      {"Payload-Checksum", "校验失败"},
                                                      {"Undeclared-History-Only-Capability", "声明遗漏"},
                                                      {"Future-Legacy-Metadata", "元数据版本"},
                                                      {"Future-Inactive-Playlist", "audio.warp.mapping v999"}};
    check(error.find(expected.at(name)) != std::string::npos,
          "actual refusal identifies the intended incompatibility rather than an unrelated checksum or parse error");
    check(c.sessionToken() == token && c.query()["tracks"] == before,
          "failed open preserves current Edit and session identity");
    rejected.push_back({{"name", name}, {"path", path.getFullPathName().toStdString()}, {"error", error}});
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("P3-ProjectDocument-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto media = source(root);
        const auto hash = Commands::mediaHash(media);
        ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("prefs")));
        auto& c = AudioDeviceTestAccess::commands(w);
        run(c, "track.create", {{"name", "Compatibility source"}, {"ref", "$t"}});
        const std::string track = c.query()["tracks"][0]["id"];
        run(c, "track.gain", {{"track", track}, {"db", 0}});
        run(c, "clip.import",
            {{"track", track}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
        const std::string clip = c.query()["tracks"][0]["clips"][0]["id"];
        const auto plain = root.getChildFile("Simple.tracktionedit");
        const auto plainReceipt = c.save(plain);
        check(!plainReceipt["protected_format"].get<bool>() && juce::parseXML(plain)->hasTagName("EDIT"),
              "simple native project remains readable by established native readers");
        auto anchors = c.warpClip(clip)["anchors"];
        anchors.insert(
            anchors.begin() + 1,
            Json{{"id", "warp:" + clip + ":middle"}, {"source_time_seconds", .5}, {"clip_time_samples", 14400}});
        const auto warp = run(c, "clip.warp", {{"clip", clip}, {"enabled", true}, {"anchors", anchors}});
        const auto split = run(c, "clip.split", {{"clip", clip}, {"position_samples", 24000}, {"ref", "$right"}});
        const auto pcm = render(c, root);
        const auto protectedFile = root.getChildFile("Processed.tracktionedit");
        const auto saved = c.save(protectedFile);
        const auto document = juce::parseXML(protectedFile);
        check(saved["protected_format"] && document->hasTagName("FORMA_PROJECT") &&
                  saved["required_capabilities"]["audio.warp.mapping"] == 2,
              "real processed window is saved inside guarded format with actual required version");
        const auto native = project_document::read(*document).edit;
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("cold")));
            cold.open(protectedFile);
            compare(pcm, render(cold, root));
            cold.undo(split["plan_id"]);
            cold.redo();
            compare(pcm, render(cold, root));
        }
        c.undo(split["plan_id"]);
        c.undo(warp["plan_id"]);
        const auto redoFile = root.getChildFile("Plain-With-Processed-Redo.tracktionedit");
        const auto redoReceipt = c.save(redoFile);
        check(redoReceipt["protected_format"] && redoReceipt["required_capabilities"]["audio.warp.mapping"] == 2 &&
                  !c.query()["tracks"][0]["clips"][0]["warp"]["enabled"].get<bool>(),
              "inactive Redo history alone requires full processing capability even when current audio is plain");
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("redo")));
            cold.open(redoFile);
            cold.redo();
            cold.redo();
            compare(pcm, render(cold, root));
        }
        auto unknown = std::make_unique<juce::XmlElement>(*document);
        auto* future = unknown->getChildByName("REQUIRES")->createNewChildElement("FEATURE");
        future->setAttribute("id", "future.required.engine");
        future->setAttribute("version", 1);
        refuse(c, *unknown, root, "Unknown-Capability");
        auto version = std::make_unique<juce::XmlElement>(*document);
        version->setAttribute("schema", 99);
        refuse(c, *version, root, "Future-Envelope");
        auto newer = std::make_unique<juce::XmlElement>(*document);
        newer->getChildByName("REQUIRES")->getFirstChildElement()->setAttribute("version", 999);
        refuse(c, *newer, root, "Future-Capability-Version");
        auto missing = std::make_unique<juce::XmlElement>(*document);
        missing->removeChildElement(missing->getChildByName("REQUIRES"), true);
        refuse(c, *missing, root, "Missing-Requirements");
        auto duplicate = std::make_unique<juce::XmlElement>(*document);
        auto* req = duplicate->getChildByName("REQUIRES");
        req->addChildElement(new juce::XmlElement(*req->getFirstChildElement()));
        refuse(c, *duplicate, root, "Duplicate-Capability");
        auto tampered = std::make_unique<juce::XmlElement>(*document);
        tampered->getChildByName("EDIT")->setAttribute("name", "Changed after checksum");
        refuse(c, *tampered, root, "Payload-Checksum");
        auto understated = juce::parseXML(redoFile);
        auto* only = understated->getChildByName("REQUIRES");
        only->deleteAllChildElements();
        auto* irrelevant = only->createNewChildElement("FEATURE");
        irrelevant->setAttribute("id", "audio.fade.inherited");
        irrelevant->setAttribute("version", 1);
        refuse(c, *understated, root, "Undeclared-History-Only-Capability");
        auto metadata = native.createCopy();
        auto metadataInfo = metadata.getChildWithName("NATIVEDAW");
        metadataInfo.removeChild(metadataInfo.getChildWithName("PERSISTENT_HISTORY"), nullptr);
        metadataInfo.setProperty("schema", 99, nullptr);
        refuse(c, *preciseEditXml(metadata), root, "Future-Legacy-Metadata");
        auto inactive = project_document::read(*juce::parseXML(plain)).edit;
        auto inactiveInfo = inactive.getChildWithName("NATIVEDAW");
        inactiveInfo.removeChild(inactiveInfo.getChildWithName("PERSISTENT_HISTORY"), nullptr);
        juce::ValueTree lists("NDAW_PLAYLISTS"), playlist("PLAYLIST"), stored("AUDIOCLIP"), futureWarp("NDAW_WARP");
        futureWarp.setProperty("schema", 999, nullptr);
        stored.addChild(futureWarp, -1, nullptr);
        playlist.addChild(stored, -1, nullptr);
        lists.addChild(playlist, -1, nullptr);
        inactive.getChildWithName("TRACK").addChild(lists, -1, nullptr);
        refuse(c, *preciseEditXml(inactive), root, "Future-Inactive-Playlist");
        auto legacy = native.createCopy();
        auto meta = legacy.getChildWithName("NATIVEDAW");
        meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
        const auto legacyFile = root.getChildFile("Legacy-Processed.tracktionedit");
        check(preciseEditXml(legacy)->writeTo(legacyFile),
              "owned pre-envelope legacy fixture writes native Edit without forged modern history");
        const auto legacyHash = Commands::mediaHash(legacyFile);
        c.open(legacyFile);
        compare(pcm, render(c, root));
        const auto migrated = root.getChildFile("Migrated-Processed.tracktionedit");
        check(c.save(migrated)["protected_format"] && Commands::mediaHash(legacyFile) == legacyHash,
              "Save Copy migrates legacy processing without modifying original file");
        const auto snapshot = AudioDeviceTestAccess::snapshot(c);
        const auto recovered = recovery::write(root.getChildFile("recovery"), {snapshot.first, snapshot.second});
        const auto loaded = recovery::read(root.getChildFile("recovery"), recovered["id"], recovered["sha256"]);
        project_document::Requirements actual;
        project_document::collect(loaded.state, actual);
        check(actual.at("audio.warp.mapping") == 2,
              "real recovery writer and reader preserve guarded full processing requirement");
        check(Commands::mediaHash(media) == hash,
              "source media stays byte-identical through all real edits, saves and recovery");
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"pcm_max_error", pcmError},
                    {"pcm_tolerance", 2e-6},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"simple", plain.getFullPathName().toStdString()},
                    {"protected", protectedFile.getFullPathName().toStdString()},
                    {"history_only", redoFile.getFullPathName().toStdString()},
                    {"migrated", migrated.getFullPathName().toStdString()},
                    {"rejected", rejected},
                    {"scope", "real L1/native engine, exact PCM, cold original history, file/recovery integrity; "
                              "physical GUI/listening separate"}};
        if (argc > 1)
            std::ofstream(argv[1]) << result.dump(2) << '\n';
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        Json result{{"result", "failed"},
                    {"checks", checks},
                    {"error", e.what()},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"rejected", rejected}};
        if (argc > 1)
            std::ofstream(argv[1]) << result.dump(2) << '\n';
        std::cerr << result.dump(2) << std::endl;
        return 1;
    }
}
