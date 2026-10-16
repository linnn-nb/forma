// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectFixture.h"
#include "HistoryRuntime.h"
#include "PersistentHistory.h"
#include <juce_cryptography/juce_cryptography.h>
#include <nativedaw/v2/EngineCommands.h>
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2
{
class PerformanceTestAccess
{
public:
    static void timestamp(Commands& c)
    {
        c.edit->markAsChanged();
    }
    static te::Engine& engine(Commands& c)
    {
        return c.engine;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File folder) : PropertyStorage("Forma history runtime tests"), folder(std::move(folder)) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
Json run(Commands& c, Json operations)
{
    return c.commit(c.makePlan("human", std::move(operations)));
}
juce::ValueTree read(const juce::File& file)
{
    return juce::ValueTree::fromXml(*project_fixture::nativeXml(file));
}
juce::ValueTree history(const juce::ValueTree& tree)
{
    return tree.getChildWithName("NATIVEDAW").getChildWithName("PERSISTENT_HISTORY");
}
Json steps(const juce::ValueTree& tree)
{
    Json result = Json::array();
    for (auto step : history(tree))
        if (step.hasType("STEP"))
            result.push_back({{"plan", step["plan_id"].toString().toStdString()},
                              {"xml", step["xml"].toString().toStdString()},
                              {"sha256", step["sha256"].toString().toStdString()}});
    return result;
}
juce::String xml(const juce::ValueTree& tree, bool compact = false)
{
    auto value = preciseEditXml(tree);
    return compact ? value->toString(juce::XmlElement::TextFormat{}.singleLine()) : value->toString();
}
juce::String hash(const juce::String& data)
{
    return juce::SHA256(data.toRawUTF8(), size_t(data.getNumBytesAsUTF8())).toHexString();
}
juce::ValueTree current(juce::ValueTree state)
{
    state = state.createCopy();
    auto meta = state.getChildWithName("NATIVEDAW");
    meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
    meta.removeChild(meta.getChildWithName("UI"), nullptr);
    for (int i = meta.getNumChildren(); --i >= 0;)
        if (meta.getChild(i).hasType("REQUEST_AUDIT"))
            meta.removeChild(i, nullptr);
    meta.removeProperty("revision", nullptr);
    return state;
}
void sameEdit(juce::ValueTree a, juce::ValueTree b)
{
    a = current(a);
    b = current(b);
    history_runtime::project(a);
    history_runtime::project(b);
    check(a.isEquivalentTo(b), "entire non-runtime native Edit, including unknown fields, is preserved");
}
juce::File write(juce::ValueTree state, const juce::File& dir, const char* name)
{
    auto file = dir.getChildFile(name);
    check(project_document::write(state, {})->writeTo(file), "owned qualification document writes");
    return file;
}
void reject(Commands& c, const juce::File& file, const char* expected)
{
    const auto before = c.query();
    std::string error;
    try
    {
        c.open(file);
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }
    check(error.find(expected) != std::string::npos, "damaged history rejected at the intended integrity gate");
    check(c.query() == before, "rejected file preserves current session, revision, full facts and Undo/Redo");
}
void fixture(const juce::File& file)
{
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = format.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    juce::AudioBuffer<float> pcm(2, 48000);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < pcm.getNumSamples(); ++i)
            pcm.setSample(c, i, .2f * std::sin(juce::MathConstants<double>::twoPi * (c ? 997 : 440) * i / 48000.));
    check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "actual PCM24 fixture written");
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& file)
{
    const auto receipt = c.render(file, 0, 48000);
    check(receipt["audio_verified"] && receipt["frames"] == 48000,
          "actual native renderer returns verified WAV receipt");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(new juce::FileInputStream(file), true));
    check(reader && reader->sampleRate == 48000 && reader->lengthInSamples == 48000 && reader->numChannels == 2,
          "independent WAV decoder checks sample rate, channels and full duration");
    juce::AudioBuffer<float> result(2, 48000);
    check(reader->read(&result, 0, 48000, 0, true, true), "actual output PCM decodes");
    return result;
}
void projection()
{
    juce::ValueTree before("EDIT"), transport("TRANSPORT"), plugin("PLUGIN"), opaque("TRANSPORT");
    before.setProperty("lastSignificantChange", "old", nullptr);
    before.setProperty("future_edit_property", "unchanged", nullptr);
    transport.setProperty("position", 1., nullptr);
    transport.setProperty("looping", false, nullptr);
    transport.setProperty("future_transport_setting", 23, nullptr);
    before.addChild(transport, -1, nullptr);
    plugin.setProperty("id", "71", nullptr);
    plugin.setProperty("lastSignificantChange", "opaque-before", nullptr);
    opaque.setProperty("position", 29., nullptr);
    plugin.addChild(opaque, -1, nullptr);
    before.addChild(plugin, -1, nullptr);
    auto after = before.createCopy();
    after.setProperty("lastSignificantChange", "new", nullptr);
    after.setProperty("future_edit_property", "edited", nullptr);
    after.getChild(0).setProperty("position", 5., nullptr);
    after.getChild(0).setProperty("looping", true, nullptr);
    after.getChild(0).setProperty("future_transport_setting", 24, nullptr);
    after.getChild(1).setProperty("lastSignificantChange", "opaque-after", nullptr);
    after.getChild(1).getChild(0).setProperty("position", 31., nullptr);
    auto delta = history_delta::difference(before, after);
    history_runtime::projectDelta(delta);
    history_runtime::project(before);
    history_runtime::project(after);
    auto actual = before.createCopy();
    history_delta::apply(actual, delta, true);
    check(actual.isEquivalentTo(after),
          "projection retains every unknown and plugin property, loop setting and full structure");
    history_delta::apply(actual, delta, false);
    check(actual.isEquivalentTo(before), "projected delta exactly reverses all non-runtime data");
    auto structural = before.createCopy();
    structural.removeChild(0, nullptr);
    auto removal = history_delta::difference(after, structural);
    // Exercise a legacy structural image, not only property changes.
    for (auto change : removal)
        for (auto child : change)
            if (child.hasType("REMOVE") && child.getChild(0).hasType("TRANSPORT"))
                child.getChild(0).setProperty("position", 99., nullptr);
    history_runtime::projectDelta(removal);
    actual = after.createCopy();
    history_delta::apply(actual, removal, true);
    check(actual.isEquivalentTo(structural),
          "structural removal projects only the root native transport runtime field");
    history_delta::apply(actual, removal, false);
    check(actual.isEquivalentTo(after), "structural Undo retains unknown transport and opaque plugin data");
}
void runtimeBoundary(const juce::File& dir)
{
    auto source = dir.getChildFile("Original.wav");
    fixture(source);
    const auto originalHash = Commands::mediaHash(source);
    Commands c(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
    run(c, Json::array(
               {op("track.create", {{"name", "Runtime boundary"}, {"ref", "$voice"}}),
                op("clip.import",
                   {{"track", "$voice"}, {"path", source.getFullPathName().toStdString()}, {"position_samples", 0}})}));
    const std::string track = c.query()["tracks"][0]["id"];
    run(c, Json::array({op("track.gain", {{"track", track}, {"db", -6}})}));
    const auto first = dir.getChildFile("before.tracktionedit");
    c.save(first);
    const auto original = read(first);
    check(int(history(original)["schema"]) == 3, "new history explicitly uses runtime-separated schema3");
    int64_t position = 0;
    for (int i = 0; i < 12; ++i)
    {
        position = 77385 + i;
        c.seek(position);
        PerformanceTestAccess::timestamp(c);
        const auto second = dir.getChildFile("seek-" + juce::String(i) + ".tracktionedit");
        c.save(second);
        const auto sought = read(second);
        check(steps(original) == steps(sought), "seek, native timestamp and save preserve every step byte and digest");
        check(std::llround(double(sought.getChildWithName("TRANSPORT")["position"]) * 48000.) == position,
              "current native saved Edit retains the actual playhead");
        sameEdit(original, sought);
    }
    const auto tracks = c.query()["tracks"];
    c.undo();
    check(std::abs(c.query()["tracks"][0]["base_gain_db"].get<double>()) < 1e-5 &&
              c.query()["position_samples"] == position,
          "live native Undo restores gain and leaves the current playhead alone");
    c.redo();
    check(c.query()["tracks"] == tracks && c.query()["position_samples"] == position,
          "live Redo restores full track facts without seeking");
    const auto audio = render(c, dir.getChildFile("before.wav"));
    const auto final = dir.getChildFile("final.tracktionedit");
    c.save(final);
    check(steps(original) == steps(read(final)), "rendering and Undo/Redo do not grow or rewrite edit history");
    {
        Commands cold(false, std::make_unique<Storage>(dir.getChildFile("cold-prefs")));
        cold.open(final);
        check(cold.query()["tracks"] == tracks && cold.query()["position_samples"] == position,
              "fresh engine restores complete track facts and saved runtime position");
        cold.seek(12345);
        cold.undo();
        check(std::abs(cold.query()["tracks"][0]["base_gain_db"].get<double>()) < 1e-5 &&
                  cold.query()["position_samples"] == 12345,
              "cold original Undo changes sound gain without restoring the historical cursor");
        auto redo = dir.getChildFile("redo-branch.tracktionedit");
        cold.save(redo);
        check(steps(original) == steps(read(redo)), "saving at an Undo boundary retains both exact original deltas");
        cold.open(redo);
        cold.seek(23456);
        cold.redo();
        check(cold.query()["tracks"] == tracks && cold.query()["position_samples"] == 23456,
              "saved Redo branch restores actual edit and preserves a later user seek");
        const auto output = render(cold, dir.getChildFile("cold.wav"));
        double error = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < audio.getNumSamples(); ++i)
                error = std::max(error, std::abs(double(audio.getSample(ch, i)) - output.getSample(ch, i)));
        check(error <= 2e-6,
              "full 48000-frame stereo PCM matches cold original Undo/Redo within unchanged 2e-6 tolerance");
        auto copy = dir.getChildFile("cold.tracktionedit");
        cold.save(copy);
        check(steps(original) == steps(read(copy)), "cold seek, save and render retain original raw step payloads");
    }
    c.seek(0);
    te::HostedAudioDeviceInterface::Parameters parameters;
    parameters.sampleRate = 48000;
    parameters.blockSize = 256;
    parameters.inputChannels = 0;
    parameters.outputChannels = 2;
    {
        te::test_utilities::EnginePlayer player(PerformanceTestAccess::engine(c), parameters);
        c.play();
        auto pcm = player.process(16000);
        c.stop();
        check(pcm.getMagnitude(0, pcm.getNumSamples()) > .01f,
              "native hosted playback produces actual non-silent source PCM");
    }
    auto played = dir.getChildFile("played.tracktionedit");
    c.save(played);
    check(steps(original) == steps(read(played)), "native playback and stop do not append runtime movement to history");
    check(Commands::mediaHash(source) == originalHash,
          "all saves, seeks, cold replay and renders preserve original media bytes");
}
juce::ValueTree legacy(juce::ValueTree modern)
{
    modern = modern.createCopy();
    auto archive = history(modern);
    archive.setProperty("schema", 2, nullptr);
    auto last = archive.getChild(archive.getNumChildren() - 2); // final STEP, before BLOCKS
    check(last.hasType("STEP"), "legacy qualification edits the actual final STEP");
    auto delta = juce::ValueTree::fromXml(*juce::parseXML(last["xml"].toString()));
    const auto state = current(modern);
    auto earlier = state.createCopy();
    earlier.setProperty("lastSignificantChange", "historical-time", nullptr);
    earlier.getChildWithName("TRANSPORT").setProperty("position", "0", nullptr);
    auto runtime = history_delta::difference(earlier, state);
    check(runtime.getNumChildren() == 2, "legacy fixture contains both real historical runtime changes");
    for (auto change : runtime)
        delta.addChild(change.createCopy(), -1, nullptr);
    const auto encoded = xml(delta, true);
    last.setProperty("xml", encoded, nullptr);
    last.setProperty("sha256", hash(encoded), nullptr);
    archive.setProperty("current_hash", hash(xml(state)), nullptr);
    return modern;
}
void migration(const juce::File& dir)
{
    const auto modernFile = dir.getChildFile("final.tracktionedit");
    const auto modern = read(modernFile);
    auto old = legacy(modern);
    const auto oldFile = write(old, dir, "legacy2.tracktionedit");
    const auto oldHash = Commands::mediaHash(oldFile);
    for (int schema : {1, 2})
    {
        auto fixture = old.createCopy();
        if (schema == 1)
        {
            // Produce the old full-state representation from a validated native
            // delta chain, retaining its original runtime values and hashes.
            std::vector<juce::ValueTree> states;
            auto state = current(fixture);
            states.push_back(state.createCopy());
            auto original = history(fixture);
            for (int i = original.getNumChildren() - 2; i >= 0; --i)
            {
                auto delta = juce::ValueTree::fromXml(*juce::parseXML(original.getChild(i)["xml"].toString()));
                history_delta::apply(state, delta, false);
                states.push_back(state.createCopy());
            }
            juce::ValueTree archive("PERSISTENT_HISTORY");
            archive.setProperty("schema", 1, nullptr);
            archive.setProperty("cursor", int(states.size()) - 1, nullptr);
            archive.setProperty("current_hash", hash(xml(current(fixture))), nullptr);
            for (size_t i = 0; i < states.size(); ++i)
            {
                juce::ValueTree entry("STATE");
                if (i)
                    entry.setProperty("plan_id", original.getChild(int(i) - 1)["plan_id"], nullptr);
                const auto data = xml(states[states.size() - i - 1]);
                entry.setProperty("xml", data, nullptr);
                entry.setProperty("sha256", hash(data), nullptr);
                archive.addChild(entry, -1, nullptr);
            }
            auto meta = fixture.getChildWithName("NATIVEDAW");
            meta.removeChild(original, nullptr);
            meta.addChild(archive, -1, nullptr);
        }
        const auto file = write(fixture, dir, schema == 1 ? "legacy1.tracktionedit" : "legacy2-copy.tracktionedit");
        const auto sourceHash = Commands::mediaHash(file);
        Commands c(false, std::make_unique<Storage>(dir.getChildFile("legacy-prefs-" + juce::String(schema))));
        c.open(file);
        const auto tracks = c.query()["tracks"];
        check(tracks.size() == 1 && c.query()["can_undo"], "checked legacy schema opens with actual native history");
        c.seek(93457);
        c.undo();
        check(std::abs(c.query()["tracks"][0]["base_gain_db"].get<double>()) < 1e-5 &&
                  c.query()["position_samples"] == 93457,
              "migrated legacy Undo restores original edit without replaying historical runtime position");
        c.redo();
        check(c.query()["tracks"] == tracks && c.query()["position_samples"] == 93457,
              "migrated legacy Redo preserves both full audio facts and current cursor");
        const auto copy = dir.getChildFile("migrated-" + juce::String(schema) + ".tracktionedit");
        c.save(copy);
        const auto migrated = read(copy);
        check(int(history(migrated)["schema"]) == 3, "explicit new Save migrates the checked old history to schema3");
        sameEdit(modern, migrated);
        const auto expected = steps(modern), actual = steps(migrated);
        check(expected.size() == actual.size(), "legacy migration retains every original Plan and edit step");
        for (size_t i = 0; i < expected.size(); ++i)
        {
            auto a = juce::ValueTree::fromXml(*juce::parseXML(juce::String(expected[i]["xml"].get<std::string>())));
            auto b = juce::ValueTree::fromXml(*juce::parseXML(juce::String(actual[i]["xml"].get<std::string>())));
            check(expected[i]["plan"] == actual[i]["plan"] && a.isEquivalentTo(b),
                  "legacy migration retains the entire original non-runtime delta and Plan ID");
        }
        c.open(copy);
        c.seek(8888);
        c.undo();
        c.redo();
        check(c.query()["tracks"] == tracks && c.query()["position_samples"] == 8888,
              "migrated saved file cold-opens with executable original Undo/Redo and independent cursor");
        check(Commands::mediaHash(file) == sourceHash, "legacy source document remains byte-identical");
    }
    Commands c(false, std::make_unique<Storage>(dir.getChildFile("rejection-prefs")));
    c.open(modernFile);
    auto corrupted = old.createCopy();
    auto step = history(corrupted).getChild(1);
    step.setProperty("xml", step["xml"].toString().replace("historical-time", "tampered-time"), nullptr);
    reject(c, write(corrupted, dir, "bad-legacy-digest.tracktionedit"), "checksum mismatch");
    corrupted = old.createCopy();
    corrupted.getChildWithName("TRANSPORT").setProperty("position", 999., nullptr);
    reject(c, write(corrupted, dir, "bad-legacy-current.tracktionedit"), "saved Edit and history disagree");
    corrupted = old.createCopy();
    step = history(corrupted).getChild(1);
    auto delta = juce::ValueTree::fromXml(*juce::parseXML(step["xml"].toString()));
    for (auto change : delta)
        if (change["path"].toString() == "[]")
            change.getChildWithName("AFTER").setProperty("lastSignificantChange", "not-the-current-value", nullptr);
    step.setProperty("xml", xml(delta, true), nullptr);
    step.setProperty("sha256", hash(xml(delta, true)), nullptr);
    reject(c, write(corrupted, dir, "bad-legacy-chain.tracktionedit"), "delta chain does not restore current Edit");
    corrupted = old.createCopy();
    history(corrupted).setProperty("schema", 3, nullptr);
    reject(c, write(corrupted, dir, "runtime-in-schema3.tracktionedit"), "runtime property in v3 edit history");
    auto raw = juce::parseXML(modernFile);
    check(raw && raw->hasTagName("FORMA_PROJECT"), "new plain project is guarded for history reader compatibility");
    auto* requirementsNode = raw->getChildByName("REQUIRES");
    requirementsNode->getFirstChildElement()->setAttribute("version", 2);
    const auto future = dir.getChildFile("future-reader.tracktionedit");
    check(raw->writeTo(future), "owned future capability fixture writes");
    reject(c, future, "history.runtime-boundary v2");
    check(Commands::mediaHash(oldFile) == oldHash, "all rejection tests leave the original legacy file untouched");
}
void editableTransport(const juce::File& dir)
{
    Commands c(false, std::make_unique<Storage>(dir.getChildFile("transport-prefs")));
    run(c,
        Json::array({op("session.range.set", {{"start_samples", 4800}, {"end_samples", 96000}}),
                     op("transport.loop.set", {{"enabled", true}}), op("transport.metronome.set", {{"enabled", true}}),
                     op("transport.count_in.set", {{"mode", "one_bar"}})}));
    const auto loop = c.query()["transport_settings"];
    run(c, Json::array({op("transport.punch.set", {{"enabled", true}})}));
    const auto punch = c.query()["transport_settings"];
    check(loop["loop_enabled"] && !loop["punch_enabled"] && !punch["loop_enabled"] && punch["punch_enabled"],
          "real L1 commands retain native exclusive Loop and Punch modes");
    c.seek(34567);
    auto file = dir.getChildFile("transport.tracktionedit");
    c.save(file);
    const auto before = steps(read(file));
    c.open(file);
    c.seek(45678);
    c.undo();
    check(c.query()["transport_settings"] == loop && c.query()["position_samples"] == 45678,
          "cold Undo restores entire loop range, metronome and count-in settings without moving playhead");
    c.redo();
    check(c.query()["transport_settings"] == punch && c.query()["position_samples"] == 45678,
          "cold Redo restores full Punch mode while preserving the newer cursor");
    c.undo();
    c.undo();
    check(!c.query()["transport_settings"]["metronome_enabled"].get<bool>() &&
              !c.query()["transport_settings"]["loop_enabled"].get<bool>() &&
              c.query()["transport_settings"]["count_in_mode"] == "none",
          "all editable transport settings still belong to their original native Undo transaction");
    c.redo();
    c.redo();
    auto copy = dir.getChildFile("transport-redone.tracktionedit");
    c.save(copy);
    check(before == steps(read(copy)),
          "editable transport replay retains exact original deltas after fresh-engine open");
}
void existingProject(const juce::File& source, const juce::File& dir, int index)
{
    const auto originalHash = Commands::mediaHash(source);
    const auto original = read(source);
    const auto expected = PersistentHistory::read(original, source);
    Commands c(false, std::make_unique<Storage>(dir.getChildFile("existing-prefs-" + juce::String(index))));
    c.open(source);
    const auto originalFacts = c.query()["tracks"];
    const auto first = dir.getChildFile("existing-" + juce::String(index) + ".tracktionedit");
    c.save(first);
    const auto migrated = read(first);
    sameEdit(original, migrated);
    const auto actual = PersistentHistory::read(migrated, first);
    check(expected.ids == actual.ids && expected.cursor == actual.cursor &&
              expected.deltas.size() == actual.deltas.size(),
          "existing local project keeps all original Plan IDs, cursor and edit deltas");
    for (size_t i = 0; i < expected.deltas.size(); ++i)
        check(expected.deltas[i].isEquivalentTo(actual.deltas[i]),
              "existing local project preserves each entire non-runtime delta, including opaque plugin data");
    const bool canUndo = c.query()["can_undo"];
    c.seek(77385);
    if (canUndo)
    {
        c.undo();
        check(c.query()["position_samples"] == 77385, "existing original Undo preserves a fresh user seek");
        c.redo();
    }
    check(c.query()["tracks"] == originalFacts && c.query()["position_samples"] == 77385,
          "existing original Undo/Redo preserves complete track facts and current cursor");
    const auto second = dir.getChildFile("existing-redone-" + juce::String(index) + ".tracktionedit");
    c.save(second);
    check(steps(migrated) == steps(read(second)),
          "existing migrated raw step bytes remain stable through seek and Undo/Redo");
    sameEdit(migrated, read(second));
    check(Commands::mediaHash(source) == originalHash, "existing source project is not overwritten by qualification");
    std::cout << "EXISTING_PROJECT "
              << Json{{"source", source.getFullPathName().toStdString()},
                      {"source_sha256", originalHash},
                      {"migrated", first.getFullPathName().toStdString()},
                      {"redone", second.getFullPathName().toStdString()},
                      {"original_plans", expected.ids.size()}}
                     .dump()
              << std::endl;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto write = [&](const Json& value)
    {
        if (argc > 1)
        {
            std::ofstream report(argv[1]);
            report << value.dump(2) << '\n';
            if (!report)
                throw std::runtime_error("qualification report could not be written");
        }
    };
    write({{"result", "running"}});
    try
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("forma-history-runtime-" + juce::Uuid().toString());
        dir.createDirectory();
        projection();
        runtimeBoundary(dir);
        migration(dir);
        editableTransport(dir);
        for (int i = 2; i < argc; ++i)
            existingProject(juce::File(juce::String::fromUTF8(argv[i])), dir, i - 2);
        const Json result{{"result", "passed"}, {"checks", checks}, {"fixture", dir.getFullPathName().toStdString()}};
        write(result);
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        write({{"result", "failed"}, {"checks", checks}, {"error", e.what()}});
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
