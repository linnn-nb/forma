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
Json samples(Commands& c, const std::string& track, const std::string& parameter, int64_t first, int64_t end,
             int stride)
{
    Json result = Json::array();
    for (auto at = first; at <= end;)
    {
        const auto last = std::min(end, at + int64_t(4095) * stride);
        auto part = last == at ? Json::array({c.automationCurveRange(track, parameter, at, at + 1, 2)[0]})
                               : c.automationCurveRange(track, parameter, at, last, int((last - at) / stride) + 1);
        for (const auto& sample : part)
            result.push_back(sample);
        at = last + stride;
    }
    return result;
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto folder = parent.getChildFile("automation-paste-" + juce::Uuid().toString());
    folder.createDirectory();
    Json cases = Json::array();
    double maxCurve = 0, maxPcm = 0;
    try
    {
        const auto media = folder.getChildFile("ActualStereo.wav");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = format.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        check(bool(writer), "real stereo source writer opened");
        juce::AudioBuffer<float> source(2, 240000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 240000; ++i)
                source.setSample(
                    ch, i, float(.05 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * i / 48000.)));
        check(writer->writeFromAudioSampleBuffer(source, 0, 240000), "actual PCM written to owned media");
        writer.reset();
        const auto mediaHash = Commands::mediaHash(media);
        for (double shape : {0., .25, -.25, .5, -.5, .75, -.75, 1., -1.})
        {
            Commands c(false);
            run(c, Json::array(
                       {op("track.create", {{"name", "Paste"}, {"ref", "$t"}}),
                        op("clip.import",
                           {{"track", "$t"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}}),
                        op("plugin.insert", {{"track", "$t"}, {"type", "4bandEq"}})}));
            pump();
            const auto setup = c.query();
            const std::string t = setup["tracks"][0]["id"], clip = setup["tracks"][0]["clips"][0]["id"];
            std::string eq;
            for (const auto& p : setup["tracks"][0]["plugins"])
                if (p["type"] == "4bandEq")
                    eq = p["id"].get<std::string>() + "::Mid gain 1";
            check(!eq.empty(), "actual EQ parameter instance enumerated");
            const std::vector<std::string> parameters{"volume", "pan", eq};
            Json operations = Json::array();
            for (const auto& parameter : parameters)
            {
                const auto values = parameter == "volume" ? std::vector<double>{-15, -3, -18, -6, -12}
                                    : parameter == "pan"  ? std::vector<double>{-.6, .4, -.3, .5, -.2}
                                                          : std::vector<double>{-3, 3, -2, 2, -1};
                int n = 0;
                for (int64_t at : {0, 36000, 84000, 144000, 240000})
                    operations.push_back(add(t, parameter, at, values[n++], shape));
            }
            run(c, operations);
            pump();
            const auto original = persistent(c, t), originalClips = c.query()["tracks"][0]["clips"];
            Json originals = Json::object(), extras = Json::object();
            for (const auto& parameter : parameters)
            {
                originals[parameter] = samples(c, t, parameter, 0, 240000, 16);
                for (int64_t boundary : {0, 36000, 84000, 96000, 144000, 192000, 288000})
                    for (int offset : {-1, 1})
                    {
                        const auto pos = boundary + offset;
                        if (pos < 0 || pos > 288000)
                            continue;
                        const auto old = pos < 96000 ? pos : pos < 144000 ? pos - 96000 + 36000 : pos - 48000;
                        extras[parameter][std::to_string(pos)] =
                            c.automationCurveRange(t, parameter, old, old + 1, 2)[0]["value"];
                    }
            }
            const auto frozen =
                c.prepareClipboard(Json::array({{{"clip", clip}, {"start_samples", 36000}, {"end_samples", 84000}}}),
                                   Json::array({t}), 36000, 84000, c.sessionToken(), c.querySummary()["revision"]);
            c.acceptClipboard(frozen["id"]);
            check(frozen["automation"].size() == 3 && persistent(c, t) == original,
                  "Copy freezes three real native curves without changing Edit");
            auto plan = c.makeClipboardPastePlan(frozen["id"], Json::array({t}), 96000, 96000, "shuffle");
            const auto preview = c.preview(plan);
            check(preview["automation_changes"].size() == 1 && preview["clip_changes"].size() == 3,
                  "dry-run includes actual crossing split, tail move, frozen insertion and automation");
            auto bad = plan;
            bad["operations"].erase(bad["operations"].begin());
            fails([&] { c.commit(bad); }, "removing compiled automation invalidates entire paste");
            bad = plan;
            bad["actor"] = "agent:external";
            fails([&] { c.commit(bad); }, "external actor cannot forge native clipboard capability");
            c.commit(plan);
            // Undo/Redo before dispatching any SDK asynchronous sort.
            c.undo();
            c.redo();
            pump();
            check(c.transactionStatus(plan["plan_id"])["state"] == "committed",
                  "immediate native Undo/Redo preserves original Plan after asynchronous clip ordering");
            const auto after = persistent(c, t), pastedClips = c.query()["tracks"][0]["clips"];
            check(pastedClips.size() == 3, "insertion keeps both crossing halves plus actual copied media");
            double error = 0;
            for (const auto& parameter : parameters)
            {
                const auto actual = samples(c, t, parameter, 0, 287952, 48);
                for (const auto& sample : actual)
                {
                    const int64_t pos = sample["position_samples"];
                    const int64_t old = pos < 96000 ? pos : pos < 144000 ? pos - 96000 + 36000 : pos - 48000;
                    const double value = sample["value"], expected = originals[parameter][old / 16]["value"];
                    error = std::max(error, parameter == "volume"
                                                ? double(std::abs(te::decibelsToVolumeFaderPosition(float(value)) -
                                                                  te::decibelsToVolumeFaderPosition(float(expected))))
                                                : std::abs(value - expected) / (parameter == "pan" ? 2. : 40.));
                }
            }
            std::cout << "shape " << shape << " paste curve error " << error << std::endl;
            check(error < 4e-7,
                  "pasted and shifted DSP curves match frozen time mapping within fixed normalized budget");
            maxCurve = std::max(maxCurve, error);
            check(c.commit(plan)["replayed"] == true && persistent(c, t) == after, "retry cannot insert twice");
            c.undo();
            pump();
            check(persistent(c, t) == original && c.query()["tracks"][0]["clips"] == originalClips,
                  "single Undo restores all native curve IDs and original audio");
            c.redo();
            pump();
            check(persistent(c, t) == after && c.query()["tracks"][0]["clips"] == pastedClips,
                  "single Redo restores identical native pasted IDs");
            if (shape == .25 || shape == 1. || shape == -1.)
            {
                auto audio = render(c, folder.getChildFile("paste-" + juce::String(shape) + ".wav"), 288000);
                Commands reference(false);
                run(reference, Json::array({op("track.create", {{"name", "Independent"}, {"ref", "$r"}}),
                                            op("plugin.insert", {{"track", "$r"}, {"type", "4bandEq"}})}));
                const auto rq = reference.query();
                const std::string r = rq["tracks"][0]["id"];
                std::string req;
                for (const auto& p : rq["tracks"][0]["plugins"])
                    if (p["type"] == "4bandEq")
                        req = p["id"].get<std::string>() + "::Mid gain 1";
                for (const auto& region :
                     std::vector<std::vector<int64_t>>{{0, 96000, 0}, {36000, 84000, 96000}, {96000, 240000, 144000}})
                    run(reference,
                        Json::array({op("clip.import", {{"track", r},
                                                        {"path", media.getFullPathName().toStdString()},
                                                        {"position_samples", 0},
                                                        {"ref", "$piece"}}),
                                     op("clip.trim",
                                        {{"clip", "$piece"}, {"start_samples", region[0]}, {"end_samples", region[1]}}),
                                     op("clip.move", {{"clip", "$piece"}, {"position_samples", region[2]}})}));
                run(reference, Json::array({add(r, "volume", 0, 0, 0), add(r, "pan", 0, 0, 0), add(r, req, 0, 0, 0)}));
                const auto referenceFile = folder.getChildFile("Reference-" + juce::String(shape) + ".tracktionedit");
                reference.save(referenceFile);
                auto xml = juce::XmlDocument::parse(referenceFile);
                check(bool(xml), "owned independently arranged native reference parsed");
                auto findPlugin = [&](auto&& self, juce::XmlElement& node, const juce::String& id) -> juce::XmlElement*
                {
                    if (node.hasTagName("PLUGIN") && node.getStringAttribute("id") == id)
                        return &node;
                    for (auto* child : node.getChildIterator())
                        if (auto* found = self(self, *child, id))
                            return found;
                    return nullptr;
                };
                const auto lanes = reference.automationQuery(r);
                for (size_t param = 0; param < parameters.size(); ++param)
                {
                    const auto name = param == 2 ? std::string("Mid gain 1") : parameters[param];
                    juce::XmlElement* curve = nullptr;
                    for (const auto& lane : lanes["lanes"])
                        if (lane["parameter"] == name)
                        {
                            auto* plugin = findPlugin(findPlugin, *xml, juce::String(lane["owner"].get<std::string>()));
                            for (auto* child : plugin->getChildIterator())
                                if (child->getStringAttribute("paramID") == juce::String(name) ||
                                    child->getStringAttribute("name") == juce::String(name))
                                    curve = child;
                        }
                    check(curve != nullptr, "real reference curve owner and parameter identified");
                    curve->deleteAllChildElements();
                    std::set<int64_t> positions;
                    for (int64_t pos = 0; pos <= 288000; pos += 16)
                        positions.insert(pos);
                    for (int64_t pos : {0, 36000, 84000, 96000, 144000, 192000, 288000})
                    {
                        if (pos > 0)
                            positions.insert(pos - 1);
                        if (pos < 288000)
                            positions.insert(pos + 1);
                    }
                    for (auto pos : positions)
                    {
                        const auto mapped = pos < 96000 ? pos : pos < 144000 ? pos - 96000 + 36000 : pos - 48000;
                        const double value = mapped % 16 == 0
                                                 ? originals[parameters[param]][mapped / 16]["value"].get<double>()
                                                 : extras[parameters[param]][std::to_string(pos)].get<double>();
                        auto* point = curve->createNewChildElement("POINT");
                        point->setAttribute("t", pos / 48000.);
                        point->setAttribute(
                            "v", double(param == 0 ? te::decibelsToVolumeFaderPosition(float(value)) : float(value)));
                        point->setAttribute("c", 0.);
                        point->setAttribute("ndaw_id", juce::Uuid().toString());
                    }
                    check(curve->getNumChildElements() == int(positions.size()),
                          "full fixed dense oracle point count retained");
                }
                check(xml->writeTo(referenceFile), "dense native reference fixture written only to owned file");
                reference.open(referenceFile);
                pump();
                auto expected =
                    render(reference, folder.getChildFile("reference-" + juce::String(shape) + ".wav"), 288000);
                double pcmError = 0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 2048; i < 286000; ++i)
                        if (std::abs(i - 96000) > 2048 && std::abs(i - 144000) > 2048)
                            pcmError = std::max(pcmError,
                                                std::abs(double(audio.getSample(ch, i)) - expected.getSample(ch, i)));
                std::cout << "shape " << shape << " paste real PCM error " << pcmError << std::endl;
                check(pcmError < 2e-5,
                      "real automated stereo EQ render agrees with independently arranged native reference");
                maxPcm = std::max(maxPcm, pcmError);
            }
            // Different selection lengths test true Shuffle replacement,
            // not the old overwrite-only GUI path. Keep all shapes and budgets.
            c.undo();
            pump();
            for (int64_t removed : {24000, 96000})
            {
                auto replacement =
                    c.makeClipboardPastePlan(frozen["id"], Json::array({t}), 96000, 96000 + removed, "shuffle");
                c.commit(replacement);
                pump();
                double replacementError = 0;
                const auto outputEnd = 240000 - removed + 48000;
                for (const auto& parameter : parameters)
                    for (const auto& sample : samples(c, t, parameter, 0, outputEnd - 48, 48))
                    {
                        const int64_t pos = sample["position_samples"], old = pos < 96000 ? pos
                                                                              : pos < 144000
                                                                                  ? pos - 96000 + 36000
                                                                                  : pos - 144000 + 96000 + removed;
                        const double value = sample["value"], expected = originals[parameter][old / 16]["value"];
                        replacementError = std::max(
                            replacementError, parameter == "volume"
                                                  ? double(std::abs(te::decibelsToVolumeFaderPosition(float(value)) -
                                                                    te::decibelsToVolumeFaderPosition(float(expected))))
                                                  : std::abs(value - expected) / (parameter == "pan" ? 2. : 40.));
                    }
                check(replacementError < 4e-7,
                      "unequal-length selection replacement retains frozen and shifted DSP automation");
                maxCurve = std::max(maxCurve, replacementError);
                c.undo();
                pump();
                check(persistent(c, t) == original && c.query()["tracks"][0]["clips"] == originalClips,
                      "replacement Undo restores exact native objects");
            }
            auto zero = c.makeClipboardPastePlan(frozen["id"], Json::array({t}), 0, 0, "shuffle");
            c.commit(zero);
            pump();
            for (const auto& parameter : parameters)
            {
                const double actual = c.automationCurveRange(t, parameter, 0, 1, 2)[0]["value"],
                             expected = originals[parameter][36000 / 16]["value"];
                const double error = parameter == "volume"
                                         ? double(std::abs(te::decibelsToVolumeFaderPosition(float(actual)) -
                                                           te::decibelsToVolumeFaderPosition(float(expected))))
                                         : std::abs(actual - expected) / (parameter == "pan" ? 2. : 40.);
                check(error < 4e-7, "session-zero paste preserves exact native first-sample step behaviour");
            }
            c.undo();
            pump();
            c.commit(c.makeClipboardPastePlan(frozen["id"], Json::array({t}), 96000, 96000, "shuffle"));
            pump();
            // New insertion IDs differ after a new Plan; snapshot these actual IDs.
            const auto finalState = persistent(c, t), finalClips = c.query()["tracks"][0]["clips"];
            const auto saved = folder.getChildFile("Paste-" + juce::String(shape) + ".tracktionedit");
            c.save(saved);
            c.open(saved);
            pump();
            check(persistent(c, t) == finalState && c.query()["tracks"][0]["clips"] == finalClips &&
                      c.clipboard().is_null(),
                  "saved real pasted curves and media reopen; transient clipboard expires");
            check(Commands::mediaHash(media) == mediaHash, "paste/Undo/Redo/save never modifies source media");
            cases.push_back({{"shape", shape},
                             {"curve_error", error},
                             {"derived_points", preview["automation_changes"][0]["derived_points"]}});
        }
        {
            Commands c(false);
            run(c, Json::array(
                       {op("track.create", {{"name", "Original"}, {"ref", "$s"}}),
                        op("track.create", {{"name", "Destination"}, {"ref", "$d"}}),
                        op("clip.import",
                           {{"track", "$s"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}}),
                        op("plugin.insert", {{"track", "$s"}, {"type", "4bandEq"}})}));
            const auto q = c.query();
            const std::string source = q["tracks"][0]["id"], target = q["tracks"][1]["id"],
                              clip = q["tracks"][0]["clips"][0]["id"];
            std::string eq;
            for (const auto& plugin : q["tracks"][0]["plugins"])
                for (const auto& parameter : plugin["parameters"])
                    if (parameter["id"] == "Mid gain 1")
                        eq = plugin["id"].get<std::string>() + "::Mid gain 1";
            run(c, Json::array({add(source, "volume", 0, -6, 0), add(source, "volume", 48000, -12, 0),
                                add(source, eq, 0, 3, 0), add(source, eq, 48000, -3, 0)}));
            const auto expected = c.automationCurveRange(source, "volume", 24000, 24001, 2)[0]["value"];
            const auto copied =
                c.prepareClipboard(Json::array({{{"clip", clip}, {"start_samples", 0}, {"end_samples", 48000}}}),
                                   Json::array({source}), 0, 48000, c.sessionToken(), c.querySummary()["revision"]);
            c.acceptClipboard(copied["id"]);
            const auto before = c.query();
            fails([&] { c.makeClipboardPastePlan(copied["id"], Json::array({target}), 96000, 96000, "shuffle"); },
                  "missing destination plugin identity refuses whole audio/automation paste");
            check(c.query()["tracks"] == before["tracks"] && c.query()["revision"] == before["revision"],
                  "plugin mapping failure is atomic");
            run(c, Json::array({op("plugin.insert", {{"track", target}, {"type", "4bandEq"}}),
                                add(target, "pan", 0, -.5, 1), add(target, "pan", 96000, .5, 0),
                                add(target, "pan", 180000, .5, 0)}));
            const auto originalTargetPan = points(c, target, "pan");
            const double incomingPan = c.automationCurveRange(target, "pan", 96000, 96001, 2)[0]["value"];
            auto stale = c.makeClipboardPastePlan(copied["id"], Json::array({target}), 96000, 96000, "shuffle");
            run(c, Json::array({op("automation.clear", {{"track", source}, {"parameter", "volume"}}),
                                add(source, "volume", 0, -40, 0), op("clip.delete", {{"clip", clip}})}));
            fails([&] { c.commit(stale); }, "human source edit invalidates an already planned paste");
            auto plan = c.makeClipboardPastePlan(copied["id"], Json::array({target}), 96000, 96000, "shuffle");
            Scope scope;
            scope.mode = Permission::Preview;
            scope.targets = {target};
            scope.begin = 96000;
            scope.end = 144000;
            fails([&] { c.review(plan, scope); },
                  "whole-track curve insertion cannot silently enlarge a bounded permission");
            const auto receipt = c.commit(plan);
            pump();
            const double actual = c.automationCurveRange(target, "volume", 120000, 120001, 2)[0]["value"];
            check(std::abs(te::decibelsToVolumeFaderPosition(float(actual)) -
                           te::decibelsToVolumeFaderPosition(float(expected))) < 4e-7,
                  "destination uses frozen curve even after source curve changed and original clip deleted");
            check(std::abs(c.automationCurveRange(target, "volume", 48000, 48001, 2)[0]["value"].get<double>()) <
                          1e-5 &&
                      std::abs(c.automationCurveRange(target, "volume", 192000, 192001, 2)[0]["value"].get<double>()) <
                          1e-5,
                  "unautomated destination explicit base is retained outside pasted range");
            check(std::abs(c.automationCurveRange(target, "pan", 120000, 120001, 2)[0]["value"].get<double>() -
                           incomingPan) < 4e-7 &&
                      std::abs(c.automationCurveRange(target, "pan", 144001, 144002, 2)[0]["value"].get<double>() -
                               .5) < 4e-7,
                  "uncopied destination lane holds native incoming step value across Shuffle gap then resumes suffix");
            const auto clips = c.query()["tracks"][1]["clips"];
            check(clips.size() == 1 && receipt["objects"].size() > 0 && Commands::mediaHash(media) == mediaHash,
                  "real frozen media survives source deletion without changing original file");
            c.undo();
            pump();
            check(c.query()["tracks"][1]["clips"].empty(), "frozen cross-track paste has a single native Undo");
            check(points(c, target, "pan") == originalTargetPan,
                  "one Undo also restores unchanged destination step lane IDs and values");
            const auto committed = c.query();
            for (const auto& bad : Json::array({Json{{"schema", 4294967297ULL}}, Json{{"position_samples", -1}}}))
            {
                auto invalid = plan;
                for (auto it = bad.begin(); it != bad.end(); ++it)
                    invalid["clipboard_paste"][it.key()] = it.value();
                invalid["base_revision"] = c.querySummary()["revision"];
                fails([&] { c.preview(invalid); }, "malformed compiled clipboard descriptor rejected");
            }
            check(c.query()["tracks"] == committed["tracks"], "invalid descriptors do not alter native objects");
        }
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << Json{{"result", "passed"},
                        {"checks", checks},
                        {"cases", cases},
                        {"curve_max_error", maxCurve},
                        {"pcm_max_error", maxPcm},
                        {"budgets", {{"curve_normalized", 4e-7}, {"pcm", 2e-5}, {"split_edge_exclusion_frames", 2048}}},
                        {"fixtures", folder.getFullPathName().toStdString()},
                        {"scope", "native clipboard/DSP/Undo/save; real WAV and independently arranged native "
                                  "reference; physical GUI separate"}}
                       .dump(2);
            check(bool(out), "evidence written after actual verification");
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << " checks=" << checks << std::endl;
        return 1;
    }
}
