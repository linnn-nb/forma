#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace
{
int checks = 0;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void fails(F f, const char* why)
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
    juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
}
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
Json run(Commands& c, Json ops)
{
    return c.commit(c.makePlan("human", std::move(ops)));
}
Json add(const std::string& track, const std::string& parameter, int64_t pos, double value, double curve)
{
    return op("automation.point.add", {{"track", track},
                                       {"parameter", parameter},
                                       {"position_samples", pos},
                                       {"value", value},
                                       {"curve", curve},
                                       {"ref", "$p" + std::to_string(pos) + parameter}});
}
Json points(Commands& c, const std::string& track, const std::string& param)
{
    const auto query = c.automationQuery(track);
    for (const auto& lane : query["lanes"])
        if (lane["parameter"] == param || lane["id"] == param)
            return lane["points"];
    throw std::runtime_error("actual lane missing");
}
Json persistent(Commands& c, const std::string& track)
{
    auto q = c.automationQuery(track)["lanes"];
    for (auto& lane : q)
        for (const char* key : {"value", "explicit_value", "display", "recording"})
            lane.erase(key);
    return q;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& f, int64_t end)
{
    auto receipt = c.render(f, 0, end);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(f));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == end &&
              receipt["frames"] == end,
          "native renderer receipt agrees with independently decoded real WAV");
    juce::AudioBuffer<float> pcm(2, int(end));
    check(reader->read(&pcm, 0, int(end), 0, true, true), "actual stereo render decoded");
    return pcm;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto folder = parent.getChildFile("automation-shuffle-" + juce::Uuid().toString());
    folder.createDirectory();
    double maxCurveError = 0, maxPcmError = 0;
    Json cases = Json::array();
    try
    {
        const auto media = folder.getChildFile("ActualStereo.wav");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = format.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        check(bool(writer), "unique owned PCM source created");
        juce::AudioBuffer<float> source(2, 240000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < source.getNumSamples(); ++i)
                source.setSample(
                    ch, i, float(.05 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * i / 48000.)));
        check(writer->writeFromAudioSampleBuffer(source, 0, source.getNumSamples()), "real source PCM written");
        writer.reset();
        const auto hash = Commands::mediaHash(media);
        for (double curve : {0., .25, -.25, .5, -.5, .75, -.75, 1., -1.})
        {
            Commands c(false);
            run(c, Json::array(
                       {op("track.create", {{"name", "Vocal"}, {"ref", "$t"}}),
                        op("clip.import",
                           {{"track", "$t"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}}),
                        op("plugin.insert", {{"track", "$t"}, {"type", "4bandEq"}})}));
            const std::string t = c.query()["tracks"][0]["id"];
            std::string eq;
            const auto initial = c.query();
            for (const auto& plugin : initial["tracks"][0]["plugins"])
                for (const auto& p : plugin["parameters"])
                    if (p["id"] == "Mid gain 1")
                        eq = plugin["id"].get<std::string>() + "::Mid gain 1";
            check(!eq.empty(), "real EQ instance and parameter enumerated");
            Json ops = Json::array();
            for (const auto& param : {std::string("volume"), std::string("pan"), eq})
            {
                const auto values = param == "volume" ? std::vector<double>{-15, -3, -18, -6, -12}
                                    : param == "pan"  ? std::vector<double>{-.6, .4, -.3, .5, -.2}
                                                      : std::vector<double>{-3, 3, -2, 2, -1};
                int n = 0;
                for (int64_t at : {0, 36000, 84000, 144000, 240000})
                    ops.push_back(add(t, param, at, values[n++], curve));
            }
            run(c, ops);
            pump();
            // Parameter-space budget is fixed before testing. Fader conversion
            // is nonlinear, so compare actual native values from the real DSP iterator.
            auto beforeState = persistent(c, t);
            const auto beforeEdit = c.query()["tracks"][0]["clips"];
            Json beforeSamples = Json::object();
            for (const auto& param : {std::string("volume"), std::string("pan"), eq})
            {
                beforeSamples[param] = Json::array();
                // Exact correspondence across the selected 1–2s cut; every 48th sample.
                for (int side = 0; side < 2; ++side)
                {
                    auto samples =
                        c.automationCurveRange(t, param, side ? 96048 : 0, side ? 239952 : 47952, side ? 2999 : 1000);
                    for (const auto& sample : samples)
                        beforeSamples[param].push_back(sample);
                }
            }
            const auto beforeFile = folder.getChildFile("Before-" + juce::String(curve) + ".tracktionedit");
            c.save(beforeFile);
            const auto beforeAudio = render(c, folder.getChildFile("before-" + juce::String(curve) + ".wav"), 240000);
            // Independent reference: manually trim/import two source clips and
            // sample the ORIGINAL playback curves at every 16th session sample.
            // This keeps stateful fader/EQ history comparable; splicing already
            // processed audio is not a valid oracle for stateful processors.
            juce::AudioBuffer<float> referenceAudio;
            {
                Commands reference(false);
                run(reference,
                    Json::array(
                        {op("track.create", {{"name", "Reference"}, {"ref", "$r"}}),
                         op("clip.import", {{"track", "$r"},
                                            {"path", media.getFullPathName().toStdString()},
                                            {"ref", "$left"},
                                            {"position_samples", 0}}),
                         op("clip.trim", {{"clip", "$left"}, {"start_samples", 0}, {"end_samples", 48000}}),
                         op("clip.import", {{"track", "$r"},
                                            {"path", media.getFullPathName().toStdString()},
                                            {"ref", "$right"},
                                            {"position_samples", 0}}),
                         op("clip.trim", {{"clip", "$right"}, {"start_samples", 96000}, {"end_samples", 240000}}),
                         op("clip.move", {{"clip", "$right"}, {"position_samples", 48000}}),
                         op("plugin.insert", {{"track", "$r"}, {"type", "4bandEq"}})}));
                const auto referenceFacts = reference.query();
                const auto referenceFile = folder.getChildFile("Reference-" + juce::String(curve) + ".tracktionedit");
                reference.save(referenceFile);
                auto referenceXml = juce::XmlDocument::parse(referenceFile);
                auto originalXml = juce::XmlDocument::parse(beforeFile);
                check(referenceXml && originalXml, "owned native reference sessions parsed");
                auto findByID = [&](auto&& self, juce::XmlElement& root, const juce::String& id) -> juce::XmlElement*
                {
                    if (root.hasTagName("PLUGIN") && root.getStringAttribute("id") == id)
                        return &root;
                    for (auto* child : root.getChildIterator())
                        if (auto* found = self(self, *child, id))
                            return found;
                    return nullptr;
                };
                const auto originalLanes = c.automationQuery(t);
                for (const auto& lane : originalLanes["lanes"])
                {
                    if (lane["points"].empty())
                        continue;
                    auto* originalPlugin =
                        findByID(findByID, *originalXml, juce::String(lane["owner"].get<std::string>()));
                    juce::XmlElement* originalCurve = nullptr;
                    if (originalPlugin)
                        for (auto* child : originalPlugin->getChildIterator())
                            if (child->getStringAttribute("paramID") ==
                                    juce::String(lane["parameter"].get<std::string>()) ||
                                child->getStringAttribute("name") == juce::String(lane["parameter"].get<std::string>()))
                                originalCurve = child;
                    check(originalCurve != nullptr, "actual native parameter curve XML identified");
                    std::string referenceOwner;
                    const auto referenceLanes = reference.automationQuery(referenceFacts["tracks"][0]["id"]);
                    for (const auto& parameter : referenceLanes["lanes"])
                        if (parameter["parameter"] == lane["parameter"])
                            referenceOwner = parameter["owner"];
                    auto* referencePlugin = findByID(findByID, *referenceXml, juce::String(referenceOwner));
                    check(referencePlugin != nullptr, "independent reference parameter enumerated");
                    auto copied = std::make_unique<juce::XmlElement>(*originalCurve);
                    copied->deleteAllChildElements();
                    std::map<int64_t, float> samples;
                    const std::string param = lane["id"];
                    auto captureSamples = [&](int64_t first, int64_t last, int count)
                    {
                        for (const auto& sample : c.automationCurveRange(t, param, first, last, count))
                        {
                            const int64_t at = sample["position_samples"];
                            if (at >= 48000 && at < 96000)
                                continue;
                            const double physical = sample["value"];
                            samples[at < 48000 ? at : at - 48000] =
                                lane["parameter"] == "volume" ? te::decibelsToVolumeFaderPosition(float(physical))
                                                              : float(physical);
                        }
                    };
                    for (int64_t at = 0; at < 240000; at += 64000)
                    {
                        const int64_t last = std::min<int64_t>(at + 63984, 239984);
                        captureSamples(at, last, int((last - at) / 16 + 1));
                    }
                    captureSamples(47999, 48000, 2);
                    captureSamples(96000, 96001, 2);
                    captureSamples(239999, 240000, 2);
                    if (std::abs(curve) == 1)
                        for (const auto& point : lane["points"])
                        {
                            const int64_t at = point["position_samples"];
                            captureSamples(at, at + 1, 2); // exact native step sample, not a 16-frame ramp
                        }
                    for (const auto& [at, value] : samples)
                    {
                        auto* point = copied->createNewChildElement("POINT");
                        point->setAttribute("t", at / 48000.);
                        point->setAttribute("v", double(value));
                        point->setAttribute("c", 0.);
                        point->setAttribute("ndaw_id", juce::Uuid().toString());
                    }
                    for (auto* child = referencePlugin->getFirstChildElement(); child;)
                    {
                        auto* next = child->getNextElement();
                        if (child->getStringAttribute("paramID") ==
                                juce::String(lane["parameter"].get<std::string>()) ||
                            child->getStringAttribute("name") == juce::String(lane["parameter"].get<std::string>()))
                            referencePlugin->removeChildElement(child, true);
                        child = next;
                    }
                    referencePlugin->addChildElement(copied.release());
                }
                check(referenceXml->writeTo(referenceFile),
                      "independent dense curve fixture written only to owned file");
                reference.open(referenceFile);
                pump();
                referenceAudio =
                    render(reference, folder.getChildFile("reference-" + juce::String(curve) + ".wav"), 192000);
            }
            auto plan = c.makeShuffleRangePlan(Json::array({t}), 48000, 96000);
            const auto preview = c.preview(plan);
            check(preview["automation_changes"].size() == 1 && preview["automation_changes"][0]["lanes"].size() == 3 &&
                      persistent(c, t) == beforeState && c.query()["tracks"][0]["clips"] == beforeEdit,
                  "dry-run previews actual volume pan EQ points without changing native Edit");
            auto tampered = plan;
            for (auto& operation : tampered["operations"])
                if (operation["command"] == "automation.range.shuffle")
                    operation["args"]["state_hash"] = "forged";
            fails([&] { c.commit(tampered); }, "forged curve fingerprint rejects entire clip/automation transaction");
            fails([&] { c.makePlan("agent:external", plan["operations"]); },
                  "external actor cannot use local range-follow command");
            const auto receipt = c.commit(plan);
            pump();
            check(receipt["state"] == "committed", "real native curve and audio transaction committed");
            double curveError = 0;
            for (const auto& param : {std::string("volume"), std::string("pan"), eq})
            {
                const auto actual = points(c, t, param);
                for (const auto& lane : preview["automation_changes"][0]["lanes"])
                    if (lane["lane"] == param || (param.find("::") == std::string::npos &&
                                                  lane["lane"].get<std::string>().ends_with("::" + param)))
                    {
                        for (const auto& pt : lane["before"])
                            if (pt["time_seconds"].get<double>() >= 2.)
                            {
                                auto found = std::find_if(actual.begin(), actual.end(),
                                                          [&](const Json& p) { return p["id"] == pt["id"]; });
                                check(found != actual.end() &&
                                          (*found)["position_samples"] ==
                                              std::llround((pt["time_seconds"].get<double>() - 1) * 48000) &&
                                          (*found)["native_value"] == pt["native_value"] &&
                                          (*found)["curve"] == pt["curve"],
                                      "later original points retain ID native value and outgoing curve");
                            }
                    }
                Json afterSamples = Json::array();
                for (int side = 0; side < 2; ++side)
                {
                    auto samples =
                        c.automationCurveRange(t, param, side ? 48048 : 0, side ? 191952 : 47952, side ? 2999 : 1000);
                    for (const auto& sample : samples)
                        afterSamples.push_back(sample);
                }
                size_t sampleIndex = 0;
                for (const auto& sample : beforeSamples[param])
                {
                    const int64_t beforePos = sample["position_samples"];
                    const int64_t afterPos = beforePos < 48000 ? beforePos : beforePos - 48000;
                    const auto& actualSample = afterSamples.at(sampleIndex++);
                    if (actualSample["position_samples"] != afterPos)
                        throw std::runtime_error("test sample mapping mismatch");
                    const auto value = actualSample["value"].get<double>();
                    const auto expected = sample["value"].get<double>();
                    if (param == "volume")
                        curveError =
                            std::max(curveError, double(std::abs(te::decibelsToVolumeFaderPosition(float(value)) -
                                                                 te::decibelsToVolumeFaderPosition(float(expected)))));
                    else
                        curveError = std::max(curveError, std::abs(value - expected) / (param == "pan" ? 2. : 40.));
                }
            }
            maxCurveError = std::max(maxCurveError, curveError);
            std::cout << "curve " << curve << " native normalized error " << curveError << std::endl;
            check(curveError < 4e-7, "retained curve equals original time splice within fixed normalized tolerance");
            const auto afterAudio = render(c, folder.getChildFile("after-" + juce::String(curve) + ".wav"), 192000);
            double pcmError = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 2048; i < 190000; ++i)
                    if (std::abs(i - 48000) >= 2048)
                        pcmError = std::max(
                            pcmError, std::abs(double(afterAudio.getSample(ch, i)) - referenceAudio.getSample(ch, i)));
            maxPcmError = std::max(maxPcmError, pcmError);
            std::cout << "curve " << curve << " real PCM error " << pcmError << std::endl;
            check(pcmError < 2e-5,
                  "real automated stereo EQ mix equals independent manually edited reference within fixed budget");
            const auto after = persistent(c, t), clipsAfter = c.query()["tracks"][0]["clips"];
            check(c.commit(plan)["replayed"] == true && persistent(c, t) == after,
                  "retry cannot collapse curves twice");
            c.undo();
            pump();
            check(persistent(c, t) == beforeState && c.query()["tracks"][0]["clips"] == beforeEdit,
                  "one Undo restores all native curves IDs and audio");
            c.redo();
            pump();
            check(persistent(c, t) == after && c.query()["tracks"][0]["clips"] == clipsAfter,
                  "one Redo restores derived point IDs and audio");
            const auto saved = folder.getChildFile("AutomationShuffle-" + juce::String(curve) + ".tracktionedit");
            c.save(saved);
            c.open(saved);
            pump();
            check(persistent(c, t) == after, "saved real Edit reopens curve positions shapes and stable IDs");
            if (curve == .25)
                check(std::abs(render(c, folder.getChildFile("reopened.wav"), 192000).getRMSLevel(0, 96000, 48000) -
                               afterAudio.getRMSLevel(0, 96000, 48000)) < 2e-7,
                      "reopened automation actually controls native rendered audio");
            cases.push_back({{"curve", curve},
                             {"normalized_error", curveError},
                             {"pcm_error", pcmError},
                             {"derived_points", preview["automation_changes"][0]["derived_points"]}});
        }
        {
            Commands c(false);
            run(c, Json::array({op("track.create", {{"name", "Edges"}, {"ref", "$edge"}}),
                                op("clip.import", {{"track", "$edge"},
                                                   {"path", media.getFullPathName().toStdString()},
                                                   {"position_samples", 0}})}));
            const std::string t = c.query()["tracks"][0]["id"];
            const auto base = c.automationQuery(t)["lanes"];
            for (const auto& positions : std::vector<std::vector<int64_t>>{
                     {0, 12000}, {120000, 192000}, {52800, 76800}, {0, 240000}, {0, 36000, 36000, 144000}})
            {
                Json ops = Json::array();
                for (size_t i = 0; i < positions.size(); ++i)
                    ops.push_back(op("automation.point.add", {{"track", t},
                                                              {"parameter", "volume"},
                                                              {"position_samples", positions[i]},
                                                              {"ref", "$edge-" + std::to_string(i)},
                                                              {"value", i % 2 ? -18. : -6.},
                                                              {"curve", i % 2 ? -.25 : .25}}));
                run(c, ops);
                pump();
                const auto before = persistent(c, t), beforeClips = c.query()["tracks"][0]["clips"];
                const auto curveBefore = c.automationCurveRange(t, "volume", 0, 47952, 1000);
                const auto suffixBefore = c.automationCurveRange(t, "volume", 96048, 239952, 2999);
                const auto plan = c.makeShuffleRangePlan(Json::array({t}), 48000, 96000);
                const auto preview = c.preview(plan);
                if (positions.back() < 48000)
                    check(preview["automation_changes"].empty(),
                          "constant tail after last point needs no automation rewrite");
                run(c,
                    Json::array({op("track.comment", {{"track", t}, {"value", "human edit during curve preview"}})}));
                fails([&] { c.commit(plan); },
                      "human edit invalidates curve-follow Plan before either audio or curve changes");
                c.undo();
                pump();
                c.commit(c.makeShuffleRangePlan(Json::array({t}), 48000, 96000));
                pump();
                const auto prefix = c.automationCurveRange(t, "volume", 0, 47952, 1000);
                const auto suffix = c.automationCurveRange(t, "volume", 48048, 191952, 2999);
                double error = 0;
                for (size_t i = 0; i < prefix.size(); ++i)
                    error = std::max(
                        error,
                        double(std::abs(te::decibelsToVolumeFaderPosition(prefix[i]["value"].get<float>()) -
                                        te::decibelsToVolumeFaderPosition(curveBefore[i]["value"].get<float>()))));
                for (size_t i = 0; i < suffix.size(); ++i)
                    error = std::max(
                        error,
                        double(std::abs(te::decibelsToVolumeFaderPosition(suffix[i]["value"].get<float>()) -
                                        te::decibelsToVolumeFaderPosition(suffixBefore[i]["value"].get<float>()))));
                check(error < 4e-7,
                      "constant first/tail inside-only ramp and coincident points preserve discrete curve values");
                c.undo();
                pump();
                check(persistent(c, t) == before && c.query()["tracks"][0]["clips"] == beforeClips,
                      "edge-case Undo restores exact native points and original audio");
                c.undo();
                pump(); // remove fixture points, keeping the original source
            }
            // A legitimate imported native point may have fractional source time
            // and unrelated custom metadata. Neither may be discarded by the edit.
            run(c, Json::array({add(t, "volume", 120000, -9., .25), add(t, "volume", 192000, -18., 0.)}));
            const auto imported = folder.getChildFile("FractionalMetadata.tracktionedit");
            c.save(imported);
            auto xml = juce::XmlDocument::parse(imported);
            auto annotate = [&](auto&& self, juce::XmlElement& n) -> void
            {
                if (n.hasTagName("POINT"))
                {
                    n.setAttribute("t", n.getDoubleAttribute("t") + .25 / 48000.);
                    n.setAttribute("fixture_note", "retain-me");
                }
                for (auto* child : n.getChildIterator())
                    self(self, *child);
            };
            annotate(annotate, *xml);
            check(xml->writeTo(imported), "owned fractional imported fixture written");
            c.open(imported);
            pump();
            auto plan = c.makeShuffleRangePlan(Json::array({t}), 0, 48000);
            const auto preview = c.preview(plan);
            c.commit(plan);
            pump();
            const auto saved = folder.getChildFile("FractionalCollapsed.tracktionedit");
            c.save(saved);
            auto savedXml = juce::XmlDocument::parse(saved);
            std::map<std::string, double> retained;
            auto inspect = [&](auto&& self, juce::XmlElement& n) -> void
            {
                if (n.hasTagName("POINT") && n.getStringAttribute("fixture_note") == "retain-me")
                    retained[n.getStringAttribute("ndaw_id").toStdString()] = n.getDoubleAttribute("t");
                for (auto* child : n.getChildIterator())
                    self(self, *child);
            };
            inspect(inspect, *savedXml);
            check(retained.size() == 2, "moved original points retain arbitrary native metadata");
            for (const auto& point : preview["automation_changes"][0]["lanes"][0]["before"])
                check(std::abs(retained.at(point["id"]) - (point["time_seconds"].get<double>() - 1.)) < 1e-12,
                      "zero-start range preserves fractional native seconds through save");
            c.open(saved);
            pump();
            // Inject 65537 genuine native points through an OWNED import fixture,
            // not a test double. Enforce the documented work budget atomically.
            auto denseXml = juce::XmlDocument::parse(saved);
            auto densify = [&](auto&& self, juce::XmlElement& n) -> bool
            {
                if (n.getChildByName("POINT"))
                {
                    n.deleteAllChildElements();
                    for (int i = 0; i < 65537; ++i)
                    {
                        auto* p = n.createNewChildElement("POINT");
                        p->setAttribute("t", i / 48000.);
                        p->setAttribute("v", .5);
                        p->setAttribute("c", 0.);
                        p->setAttribute("ndaw_id", juce::Uuid().toString());
                    }
                    return true;
                }
                for (auto* child : n.getChildIterator())
                    if (self(self, *child))
                        return true;
                return false;
            };
            check(densify(densify, *denseXml), "actual native curve located for bounded-work fixture");
            const auto dense = folder.getChildFile("OverBudget.tracktionedit");
            check(denseXml->writeTo(dense), "owned 65537 native point fixture saved");
            c.open(dense);
            pump();
            const auto denseBefore = c.querySummary();
            const auto densePoints = persistent(c, t);
            fails([&] { c.makeShuffleRangePlan(Json::array({t}), 0, 48000); },
                  "65537 real native points reject entire edit before exceeding work budget");
            check(c.querySummary() == denseBefore && persistent(c, t) == densePoints,
                  "budget refusal preserves real points audio and revision");
        }
        check(Commands::mediaHash(media) == hash, "all automation edits preserve original PCM hash");
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"curve_max_normalized_error", maxCurveError},
                    {"pcm_max_error", maxPcmError},
                    {"cases", cases},
                    {"budgets", {{"curve_normalized", 4e-7}, {"pcm", 2e-5}, {"split_edge_exclusion_frames", 2048}}},
                    {"scope", "native DSP iterator and decoded real stereo WAV plus independent manual/dense "
                              "reference; volume/pan/enumerated EQ, 9 shapes, "
                              "Undo/Redo/save/open; physical GUI separate"},
                    {"fixtures", folder.getFullPathName().toStdString()}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            check(bool(out), "report written");
        }
        std::cout << report.dump(2) << std::endl;
        if (argc <= 2)
            folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
