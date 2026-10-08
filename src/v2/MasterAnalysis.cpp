#include "MasterAnalysis.h"
#include "NativePluginStates.h"
#include <nativedaw/v2/AudioAnalysis.h>
#include <nativedaw/v2/DeliveryCheck.h>
#include <nativedaw/v2/ExportVerification.h>
#include <filesystem>
#include <nativedaw/v2/SourceFeatures.h>
#include <nativedaw/v2/SourceMapping.h>
#include <nativedaw/v2/LoudnessCurve.h>
#include <set>
#include <limits>
#if JUCE_MAC
#include <pthread.h>
#endif

namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
double now()
{
    return juce::Time::getMillisecondCounterHiRes();
}
void fields(const Json& args, std::initializer_list<const char*> allowed)
{
    require(args.is_object(), "analysis arguments must be an object");
    for (auto it = args.begin(); it != args.end(); ++it)
    {
        bool ok = false;
        for (auto key : allowed)
            ok |= it.key() == key;
        require(ok, "unknown analysis field");
    }
}
int64_t integer(const Json& value)
{
    require(value.is_number_integer() && value.get<double>() >= 0 && value.get<double>() <= 9007199254740991.,
            "analysis positions must be nonnegative integer samples");
    return value.get<int64_t>();
}
std::string fingerprint(juce::ValueTree state)
{
    state = state.createCopy();
    state.removeChild(state.getChildWithName("NATIVEDAW"), nullptr);
    state.removeChild(state.getChildWithName("TRANSPORT"), nullptr);
    for (auto key : {"creationTime", "lastSignificantChange", "modifiedBy", "appVersion", "projectID"})
        state.removeProperty(key, nullptr);
    // SDK timers may reorder heterogeneous children (clips, plugins, outputs)
    // without changing sound. Preserve order WITHIN each semantic collection,
    // especially plugins and the combined audio/folder track collection.
    std::function<Json(const juce::XmlElement&)> canonical = [&](const auto& node)
    {
        Json properties = Json::object(), collections = Json::object();
        for (int i = 0; i < node.getNumAttributes(); ++i)
            properties[node.getAttributeName(i).toStdString()] = node.getAttributeValue(i).toStdString();
        // These exact mappings are the attachToCurrentValue bindings in the
        // locked SDK VolumeAndPan, Equaliser and Delay sources. Only nonempty
        // curves own a sampled cache: preserve the complete curve and revision,
        // all other properties and opaque external plugin state. Empty curves
        // still hash explicit bases. Unknown plugins remain conservative.
        static const std::map<std::string, std::map<std::string, std::string>> curveCaches = {
            {te::VolumeAndPanPlugin::xmlTypeName,
             {{"volume", "volume"}, {"master volume", "volume"}, {"pan", "pan"}, {"master pan", "pan"}}},
            {te::EqualiserPlugin::xmlTypeName,
             {{"Low-pass freq", "loFreq"},
              {"Low-pass gain", "loGain"},
              {"Low-pass Q", "loQ"},
              {"Mid freq 1", "midFreq1"},
              {"Mid gain 1", "midGain1"},
              {"Mid Q 1", "midQ1"},
              {"Mid freq 2", "midFreq2"},
              {"Mid gain 2", "midGain2"},
              {"Mid Q 2", "midQ2"},
              {"High-pass freq", "hiFreq"},
              {"High-pass gain", "hiGain"},
              {"High-pass Q", "hiQ"}}},
            {te::DelayPlugin::xmlTypeName, {{"feedback", "feedback"}, {"mix proportion", "mix"}}}};
        auto cache = curveCaches.find(node.getStringAttribute("type").toStdString());
        if (node.hasTagName("PLUGIN") && cache != curveCaches.end())
            for (auto* child = node.getFirstChildElement(); child; child = child->getNextElement())
                if (child->hasTagName("AUTOMATIONCURVE") && child->getNumChildElements() > 0)
                {
                    auto property = cache->second.find(child->getStringAttribute("paramID").toStdString());
                    if (property != cache->second.end())
                        properties.erase(property->second);
                }
        for (auto* child = node.getFirstChildElement(); child; child = child->getNextElement())
        {
            auto kind = child->getTagName().toStdString();
            if (kind == "TRACK" || kind == "FOLDERTRACK")
                kind = "TRACK_ORDER";
            if (!collections.contains(kind))
                collections[kind] = Json::array();
            collections[kind].push_back(canonical(*child));
        }
        return Json{
            {"type", node.getTagName().toStdString()}, {"properties", properties}, {"collections", collections}};
    };
    auto node = state.createXml();
    const auto text = canonical(*node).dump();
    require(text.size() <= 2 * 1024 * 1024, "analysis snapshot exceeds the current 2 MiB state budget");
    return juce::SHA256(text.data(), text.size()).toHexString().toStdString();
}
// SHA256 reads through cancellation/pause checks, including large source files.
struct HashReads
{
    std::atomic<uint64_t> files{0}, bytes{0};
};
class CheckedStream final : public juce::InputStream
{
public:
    CheckedStream(const juce::File& file, const analysis::Control& c, HashReads* reads = nullptr)
        : stream(file), control(c), reads(reads)
    {
        require(stream.openedOk(), "analysis media cannot be read");
        if (reads)
            ++reads->files;
    }
    int64_t getTotalLength() override
    {
        return stream.getTotalLength();
    }
    bool isExhausted() override
    {
        return stream.isExhausted();
    }
    int64_t getPosition() override
    {
        return stream.getPosition();
    }
    bool setPosition(int64_t at) override
    {
        return stream.setPosition(at);
    }
    int read(void* buffer, int n) override
    {
        require(!control.cancelled(), "analysis cancelled or deadline expired");
        control.yield();
        const int count = stream.read(buffer, n);
        require(count > 0 || stream.isExhausted(), "analysis media hash read failed");
        if (reads)
            reads->bytes += uint64_t(count);
        return count;
    }

private:
    juce::FileInputStream stream;
    analysis::Control control;
    HashReads* reads;
};
// Cache only within ONE validation pass. The post-render pass starts empty,
// so same-size/same-mtime byte mutations still require a fresh deep SHA256.
Json hashes(const Json& sources, const analysis::Control& control, HashReads* reads = nullptr)
{
    Json out = Json::array();
    std::map<std::string, Json> seen;
    for (auto source : sources)
    {
        require(!control.cancelled(), "analysis cancelled or deadline expired");
        control.yield();
        const juce::File file(juce::String{source["path"].get<std::string>()});
        require(file.existsAsFile(), "analysis source media missing");
        const auto size = file.getSize(), mtime = file.getLastModificationTime().toMilliseconds();
        if (source.contains("bytes"))
            require(source["bytes"] == size && source["modified_ms"] == mtime, "source changed since snapshot capture");
        const auto path = file.getFullPathName().toStdString();
        if (auto found = seen.find(path); found != seen.end())
        {
            require(found->second["bytes"] == size && found->second["modified_ms"] == mtime,
                    "shared source changed within validation pass");
            source["sha256"] = found->second["sha256"];
        }
        else
        {
            CheckedStream stream(file, control, reads);
            source["sha256"] = juce::SHA256(stream).toHexString().toStdString();
            require(size == file.getSize() && mtime == file.getLastModificationTime().toMilliseconds(),
                    "source changed while hashing");
            seen[path] = {{"bytes", size}, {"modified_ms", mtime}, {"sha256", source["sha256"]}};
        }
        source["bytes"] = size;
        source["modified_ms"] = mtime;
        out.push_back(std::move(source));
    }
    return out;
}
// Keep EVERY clip/track reference while storing a shared file descriptor once.
// This avoids exhausting the unchanged artifact byte budget on duplicated paths
// and digests in dense edits. Original first-reference fields remain available.
Json mediaManifest(const Json& sources)
{
    Json out = Json::array();
    std::map<std::string, size_t> indices;
    for (const auto& source : sources)
    {
        const auto path = source.at("path").get<std::string>();
        auto [at, inserted] = indices.emplace(path, out.size());
        if (inserted)
        {
            out.push_back(source);
            out.back()["clip_references"] = Json::array();
        }
        auto& descriptor = out.at(at->second);
        require(descriptor["bytes"] == source["bytes"] && descriptor["modified_ms"] == source["modified_ms"],
                "shared source changed while capturing references");
        Json ref = {{"clip_id", source["clip_id"]}};
        if (source.contains("track_id"))
            ref["track_id"] = source["track_id"];
        descriptor["clip_references"].push_back(std::move(ref));
    }
    return out;
}
size_t referenceCount(const Json& sources)
{
    size_t n = 0;
    for (const auto& source : sources)
        n += source.contains("clip_references") ? source["clip_references"].size() : 1;
    return n;
}
// L1-only transformation of a detached render Edit. An actual SDK send/return
// samples the chosen plugin boundary; all original routing stays connected.
// Original device outputs become sinks so unrelated tracks are not summed into
// the measurement. This is never installed in the active playback Edit.
Json prepareTrackTap(te::Edit& snapshot, const std::string& trackID, const std::string& tap)
{
    te::AudioTrack* target = nullptr;
    std::set<int> used;
    for (auto* track : te::getAudioTracks(snapshot))
    {
        if (track->itemID.toString().toStdString() == trackID)
            target = track;
        for (auto* plugin : track->pluginList)
        {
            if (auto* s = dynamic_cast<te::AuxSendPlugin*>(plugin))
                used.insert(s->getBusNumber());
            if (auto* r = dynamic_cast<te::AuxReturnPlugin*>(plugin))
                used.insert(r->busNumber.get());
            require(!dynamic_cast<te::InsertPlugin*>(plugin), "offline tap cannot certify hardware inserts");
        }
    }
    require(target, "analysis audio/instrument/Aux track not found");
    const auto fader = target->pluginList.indexOf(target->getVolumePlugin());
    require(fader >= 0, "track tap requires an actual VolumeAndPan boundary");
    int boundary = tap == "bus" ? target->pluginList.size() : fader;
    if (tap == "track_pre_inserts")
    {
        boundary = 0;
        bool effectSeen = false;
        for (int i = 0; i < fader; ++i)
        {
            auto* plugin = target->pluginList[i];
            const bool input = plugin->isSynth() || dynamic_cast<te::AuxReturnPlugin*>(plugin);
            if (input)
            {
                require(!effectSeen, "pre-insert boundary ambiguous: effect/send precedes instrument or Aux return");
                boundary = i + 1;
            }
            else if (!dynamic_cast<te::LevelMeterPlugin*>(plugin))
                effectSeen = true;
        }
    }
    const auto before = boundary < target->pluginList.size()
                            ? target->pluginList[boundary]->itemID.toString().toStdString()
                            : std::string{};
    require(target->pluginList.size() < snapshot.engine.getEngineBehaviour().getEditLimits().maxPluginsOnTrack,
            "track tap needs one temporary SDK plugin slot in the render snapshot");
    int bus = 0;
    while (used.contains(bus))
    {
        require(bus < std::numeric_limits<int>::max(), "analysis bus IDs exhausted");
        ++bus;
    }
    auto capture = snapshot.insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(snapshot), nullptr, false);
    require(capture != nullptr, "analysis capture track creation failed");
    capture->setName("Forma offline tap");
    capture->setSoloIsolate(true);
    auto returned = snapshot.getPluginCache().createNewPlugin(te::AuxReturnPlugin::xmlTypeName, {});
    require(returned != nullptr, "analysis return creation failed");
    dynamic_cast<te::AuxReturnPlugin&>(*returned).busNumber = bus;
    capture->pluginList.insertPlugin(returned, 0, nullptr);
    require(capture->pluginList.contains(returned.get()), "analysis return insertion failed");
    auto sent = snapshot.getPluginCache().createNewPlugin(te::AuxSendPlugin::xmlTypeName, {});
    require(sent != nullptr, "analysis tap creation failed");
    auto& send = dynamic_cast<te::AuxSendPlugin&>(*sent);
    send.busNumber = bus;
    send.gain->setParameter(te::decibelsToVolumeFaderPosition(0.f), juce::dontSendNotification);
    target->pluginList.insertPlugin(sent, boundary, nullptr);
    require(target->pluginList.contains(sent.get()), "analysis tap insertion failed");
    for (auto* track : te::getAllTracks(snapshot))
        if (track != capture.get())
            if (auto* output = te::getTrackOutput(*track); output && !output->getDestinationTrack())
                output->setOutputToNone();
    capture->getOutput().setOutputToDefaultDevice(false);
    return {{"method", "native AuxSend/AuxReturn in isolated render snapshot"},
            {"tap_point", tap},
            {"track_id", trackID},
            {"track_name", target->getName().toStdString()},
            {"plugin_boundary_index", boundary},
            {"before_plugin_id", before.empty() ? Json(nullptr) : Json(before)},
            {"master_included", false},
            {"fader_included", tap == "bus"},
            {"upstream_routes_and_sends_preserved", true},
            {"active_edit_modified", false},
            {"time_domain", "session samples at 48000 Hz"}};
}
// L1 strips unrelated playback structure BEFORE constructing the detached Edit,
// so neither another clip nor an upstream instrument/return/track insert can
// enter this measurement. Stable clip/plugin IDs and musical context survive.
Json isolateClipState(juce::ValueTree& state, te::WaveAudioClip& clip)
{
    auto* track = clip.getTrack();
    require(track != nullptr, "clip owner unavailable");
    for (int i = state.getNumChildren() - 1; i >= 0; --i)
    {
        auto child = state.getChild(i);
        if (child.hasType("TRACK") || child.hasType("FOLDERTRACK") || child.hasType("MASTERPLUGINS"))
            state.removeChild(i, nullptr);
    }
    juce::ValueTree isolated("TRACK");
    isolated.setProperty("id", track->state.getProperty("id"), nullptr);
    isolated.setProperty("name", "Forma isolated Clip FX", nullptr);
    isolated.addChild(clip.state.createCopy(), -1, nullptr);
    state.addChild(isolated, -1, nullptr);
    Json ids = Json::array();
    for (auto* p : *clip.getPluginList())
        ids.push_back(p->itemID.toString().toStdString());
    const auto pos = clip.getPosition();
    return {{"method", "isolated native audio clip graph"},
            {"tap_point", "clip_post_fx"},
            {"clip_id", clip.itemID.toString().toStdString()},
            {"original_track_id", track->itemID.toString().toStdString()},
            {"plugin_ids", ids},
            {"clip_gain_pan_included", true},
            {"clip_fades_included", true},
            {"other_clips_included", false},
            {"track_processing_included", false},
            {"track_mute_solo_included", false},
            {"upstream_routes_and_inputs_included", false},
            {"master_included", false},
            {"fader_included", false},
            {"active_edit_modified", false},
            {"clip_start_samples", std::llround(pos.getStart().inSeconds() * 48000)},
            {"clip_end_samples", std::llround(pos.getEnd().inSeconds() * 48000)},
            {"source_offset_seconds", pos.getOffset().inSeconds()},
            {"time_domain", "session samples at 48000 Hz"}};
}
} // namespace
struct MasterAnalysis::Job
{
    std::unique_ptr<te::Edit> snapshot;
    std::unique_ptr<te::Edit::ScopedRenderStatus> renderStatus;
    juce::WavAudioFormat wav;
    std::unique_ptr<te::Renderer::RenderTask> task;
    std::thread worker;
    std::optional<te::ScopedThreadExitStatusEnabler> exitEnabler;
    std::atomic<bool> cancel{false}, pause{false}, userPause{false}, parked{false}, done{false};
    std::atomic<int> stage{0};
    std::atomic<float> progress{0};
    std::atomic<double> hashMs{0}, renderMs{0}, measureMs{0}, validationMs{0}, pausedMs{0};
    HashReads sourceReads;
    Json preparation = Json::object();
    juce::File directory, pcm, destination, exportStage;
    Json binding, sources, result;
    std::string actor;
    double started = now();
    ~Job()
    {
        cancel = true;
        if (worker.joinable())
        {
            te::signalThreadShouldExit(worker.get_id());
            worker.join();
        }
        task.reset();
        renderStatus.reset();
        snapshot.reset();
        if (directory != juce::File{})
            directory.deleteRecursively();
    }
    bool expired() const
    {
        return now() - started > 60000.;
    }
    Json runtime() const
    {
        return {{"version", "forma-analysis-runtime/1"},
                {"preparation_ms", preparation},
                {"worker_ms",
                 {{"source_hash", hashMs.load()},
                  {"render", renderMs.load()},
                  {"measure", measureMs.load()},
                  {"final_validation", validationMs.load()},
                  {"parked", pausedMs.load()}}},
                {"source_hash_file_reads", sourceReads.files.load()},
                {"source_hash_bytes_read", sourceReads.bytes.load()},
                {"source_references", referenceCount(sources)},
                {"unique_media_files", sources.size()},
                {"timing_rule",
                 "wall-clock completed phase durations; includes parked/SDK/I/O waits; unfinished phases remain zero"},
                {"elapsed_ms", now() - started},
                {"preparation_thread", "message"},
                {"render_driver_threads", 1},
                {"sdk_graph_parallelism", "engine default; not a single-core guarantee"}};
    }
    void launch()
    {
        auto gate = std::make_shared<juce::WaitableEvent>();
        worker = std::thread(
            [this, gate]
            {
                gate->wait();
                // Published descriptors stay immutable while GUI/MCP query the job.
                auto binding = this->binding;
                auto sources = this->sources;
#if JUCE_MAC
                pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
#endif
                analysis::Control control{[this] { return cancel.load() || expired(); },
                                          [this]
                                          {
                                              const auto began = now();
                                              if (pause.load() || userPause.load())
                                              {
                                                  parked = true;
                                                  try
                                                  {
                                                      while (pause.load() || userPause.load())
                                                      {
                                                          require(!cancel.load() && !expired(),
                                                                  "analysis cancelled or deadline expired");
                                                          juce::Thread::sleep(10);
                                                      }
                                                  }
                                                  catch (...)
                                                  {
                                                      parked = false;
                                                      pausedMs.fetch_add(now() - began);
                                                      throw;
                                                  }
                                                  parked = false;
                                                  pausedMs.fetch_add(now() - began);
                                              }
                                          }};
                try
                {
                    auto span = now();
                    sources = hashes(sources, control, &sourceReads);
                    hashMs = now() - span;
                    stage = 1;
                    span = now();
                    if (binding["purpose"] == "source")
                    {
                        stage = 2;
                        result = analysis::measure(pcm, 0, control,
                                                   {binding.at("source_start_frame"), binding.at("source_end_frame")},
                                                   binding.at("detector_profile"));
                        require(result["frames"].get<int64_t>() == binding["source_end_frame"].get<int64_t>() -
                                                                       binding["source_start_frame"].get<int64_t>(),
                                "source analysis frame count mismatch");
                        binding["source_id"] = "sha256:" + sources[0]["sha256"].get<std::string>();
                        binding["media_sha256"] = sources[0]["sha256"];
                        const auto descriptor = Json{{"tap", "raw_source"},
                                                     {"media_sha256", sources[0]["sha256"]},
                                                     {"range", result["read_range"]},
                                                     {"detector_profile", binding["detector_profile"]}}
                                                    .dump();
                        binding["processing_chain_hash"] =
                            juce::SHA256(descriptor.data(), descriptor.size()).toHexString().toStdString();
                        binding["processing_scope"] = "raw source only; no clip FX, clip gain, track inserts or Master";
                        Json merged = result["source_features"]["events"];
                        for (auto event : result["events"])
                        {
                            event["source_start_frame"] = binding["source_start_frame"].get<int64_t>() +
                                                          event["render_start_frame"].get<int64_t>();
                            event["source_end_frame"] =
                                binding["source_start_frame"].get<int64_t>() + event["render_end_frame"].get<int64_t>();
                            event["estimated"] = false;
                            event.erase("start_samples");
                            event.erase("end_samples");
                            event.erase("render_start_frame");
                            event.erase("render_end_frame");
                            merged.push_back(event);
                        }
                        std::stable_sort(merged.begin(), merged.end(),
                                         [](const auto& a, const auto& b)
                                         {
                                             return a["source_start_frame"].template get<int64_t>() <
                                                    b["source_start_frame"].template get<int64_t>();
                                         });
                        const auto fullScaleCount = result["event_count"].get<int64_t>();
                        const auto& counts = result["source_features"]["event_counts"];
                        result["event_counts"] = {{"full_scale_exceedance", fullScaleCount},
                                                  {"silence", counts["silence"]},
                                                  {"transient_candidate", counts["transient_candidate"]}};
                        result["event_count"] = fullScaleCount + counts["silence"].get<int64_t>() +
                                                counts["transient_candidate"].get<int64_t>();
                        if (merged.size() > 128)
                            merged.erase(merged.begin() + 128, merged.end());
                        for (size_t i = 0; i < merged.size(); ++i)
                            merged[i]["id"] = i;
                        result["events"] = std::move(merged);
                        result["events_omitted"] =
                            result["event_count"].get<int64_t>() - int64_t(result["events"].size());
                        result["source_features"].erase("events");
                        result["parameters"]["event_rule"] = "contiguous native source-file frames with abs(sample) >= "
                                                             "1 in any channel; half-open source interval";
                        result["peak_source_frame"] = result["peak_file_frame"];
                        result.erase("peak_position_samples");
                        auto& ending = result["ending_window"];
                        ending["source_start_frame"] =
                            binding["source_end_frame"].get<int64_t>() - ending["frames"].get<int64_t>();
                        ending["source_end_frame"] = binding["source_end_frame"];
                        ending.erase("start_samples");
                        ending.erase("end_samples");
                        result["event_time_domain"] = "native source-file frames";
                    }
                    else
                    {
                        while (true)
                        {
                            require(!control.cancelled(), "analysis cancelled or deadline expired");
                            control.yield();
                            if (task->runJob() == juce::ThreadPoolJob::jobHasFinished)
                                break;
                        }
                        require(task->errorMessage.isEmpty(), task->errorMessage.toRawUTF8());
                        renderMs = now() - span;
                        span = now();
                        stage = 2;
                        if (binding["purpose"] == "export")
                            result = analysis::stageVerifiedWav(pcm, exportStage, binding.at("start_samples"),
                                                                binding["end_samples"].get<int64_t>() -
                                                                    binding["start_samples"].get<int64_t>(),
                                                                control, binding.at("delivery_profile"));
                        else
                            result = analysis::measure(pcm, binding.at("start_samples"), control, {},
                                                       binding.at("detector_profile"),
                                                       analysis::FeatureDomain::SessionSamples);
                        result["processed_event_detection_enabled"] = binding.at("detector_profile").is_object();
                        if (!result["processed_event_detection_enabled"].get<bool>())
                            result["event_counts"] = {{"full_scale_exceedance", result["event_count"]},
                                                      {"silence", nullptr},
                                                      {"transient_candidate", nullptr}};
                        require(result["frames"].get<int64_t>() ==
                                    binding["end_samples"].get<int64_t>() - binding["start_samples"].get<int64_t>(),
                                "analysis render frame count mismatch");
                        require(binding["purpose"] == "export" ||
                                    result["file_float"].get<bool>() && result["file_bits"] == 32,
                                "analysis renderer did not produce float32 PCM");
                    }
                    measureMs = now() - span;
                    span = now();
                    stage = 3;
                    if (binding["purpose"] == "delivery")
                        result["delivery"] = delivery::evaluate(result, binding.at("delivery_profile"));
                    require(hashes(sources, control, &sourceReads) == sources, "source media changed during analysis");
                    if (binding["purpose"] != "source")
                    {
                        CheckedStream stream(pcm, control);
                        result["render_sha256"] = juce::SHA256(stream).toHexString().toStdString();
                    }
                    validationMs = now() - span;
                    result["media"] = sources;
                    result["media_validation"] =
                        "independent SHA256 passes before/after decoding or render, one read per unique path per pass; "
                        "all clip references retained; size/mtime while querying; fresh deep SHA256 before locate";
                    result["binding"] = binding;
                    result["state"] = "completed";
                    result["elapsed_ms"] = now() - started;
                    result["artifact_id"] = binding["artifact_id"];
                    result["runtime"] = runtime();
                    require(result.dump().size() <= Commands::maximumQueryPageBytes - 4096,
                            "analysis artifact exceeds the current 252 KiB receipt budget");
                }
                catch (const std::exception& e)
                {
                    result = {{"state", cancel.load() ? "cancelled"
                                        : expired()   ? "expired"
                                                      : "failed"},
                              {"error", e.what()},
                              {"binding", binding},
                              {"artifact_id", binding["artifact_id"]},
                              {"runtime", runtime()}};
                }
                done.store(true, std::memory_order_release);
            });
        exitEnabler.emplace(worker.get_id());
        gate->signal();
    }
};
MasterAnalysis::MasterAnalysis(Commands& c) : owner(c)
{
    owner.checkThread();
    startTimer(50);
}
MasterAnalysis::~MasterAnalysis()
{
    stopTimer();
    job.reset();
}
void MasterAnalysis::reset()
{
    owner.checkThread();
    job.reset();
    receipt = nullptr;
    phase = "idle";
}
void MasterAnalysis::prioritizePlayback(bool active)
{
    owner.checkThread();
    if (job)
        job->pause = active;
}
std::string MasterAnalysis::chainHash() const
{
    return fingerprint(owner.edit->state);
}
bool MasterAnalysis::current(const Json& result, bool deep)
{
    if (!result.is_object() || result.value("state", std::string{}) != "completed")
        return false;
    if (result.value("invalidated", false))
        return false;
    const auto& binding = result.at("binding");
    if (binding["session_token"] != owner.sessionToken())
        return false;
    if (owner.edit->getTransport().isPlaying() || !owner.parameterCapture.is_null() ||
        owner.audioConfigurationPending())
        return false;
    if (binding["purpose"] != "source" &&
        (binding["revision"] != owner.revision || chainHash() != binding["processing_chain_hash"].get<std::string>()))
        return false;
    for (const auto& source : result.at("media"))
    {
        const juce::File file(juce::String{source["path"].get<std::string>()});
        if (!file.existsAsFile() || file.getSize() != source["bytes"] ||
            file.getLastModificationTime().toMilliseconds() != source["modified_ms"])
            return false;
    }
    if (binding["purpose"] == "export")
    {
        const auto& verified = result.at("file_verification");
        const juce::File output(juce::String{verified.at("path").get<std::string>()});
        if (!output.existsAsFile() || output.getSize() != verified["bytes"] ||
            output.getLastModificationTime().toMilliseconds() != verified["modified_ms"])
            return false;
        if (deep && Commands::mediaHash(output) != verified["sha256"].get<std::string>())
            return false;
    }
    return !deep || hashes(result.at("media"), {}) == result.at("media");
}
Json MasterAnalysis::sourceMappings(const Json& result) const
{
    Json clips = Json::array();
    int64_t total = 0;
    const juce::File source(juce::String{result["media"][0]["path"].get<std::string>()});
    for (auto* track : te::getAudioTracks(*owner.edit))
        for (auto* clip : track->getClips())
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(clip); wave && wave->getOriginalFile() == source)
            {
                ++total;
                if (clips.size() >= 128)
                    continue;
                const auto p = wave->getPosition();
                const auto speed = wave->getSpeedRatio();
                const bool supported = !wave->isLooping() && !wave->getAutoTempo() && !wave->getWarpTime() &&
                                       !wave->getIsReversed() && std::isfinite(speed) && speed > 0;
                clips.push_back({{"clip_id", clip->itemID.toString().toStdString()},
                                 {"track_id", track->itemID.toString().toStdString()},
                                 {"name", clip->getName().toStdString()},
                                 {"track_name", track->getName().toStdString()},
                                 {"available", supported},
                                 {"reason", supported ? "linear native clip mapping"
                                                      : "loop/auto-tempo/warp/reversed mapping not qualified"},
                                 {"start_samples", std::llround(p.getStart().inSeconds() * 48000)},
                                 {"end_samples", std::llround(p.getEnd().inSeconds() * 48000)},
                                 {"source_sample_rate", result.at("sample_rate")},
                                 {"source_offset_seconds", p.getOffset().inSeconds() * speed},
                                 {"speed_ratio", speed},
                                 {"mapping_revision", owner.revision}});
            }
    return {{"clips", clips},
            {"total", total},
            {"omitted", total - int64_t(clips.size())},
            {"mapping_revision", owner.revision},
            {"time_domain", "current session samples at 48000 Hz; source evidence remains in native frames"}};
}
Json MasterAnalysis::observed(Json result)
{
    if (result.is_object() && result.value("state", std::string{}) == "completed")
    {
        result["current"] = current(result);
        if (result["binding"]["purpose"] == "source")
        {
            result["mapping_set"] = sourceMappings(result);
            result["mapping_revision"] = owner.revision;
            result["validity_scope"] =
                "raw source media; track/clip gain and insert changes do not change these measurements";
        }
    }
    return result;
}
void MasterAnalysis::poll()
{
    owner.checkThread();
    if (!job)
        return;
    job->pause = owner.edit->getTransport().isPlaying() || owner.audioConfigurationPending();
    if (!job->done.load(std::memory_order_acquire))
    {
        phase = job->cancel                    ? "cancelling"
                : job->pause || job->userPause ? (job->parked ? "paused" : "pausing")
                : job->stage == 0              ? "hashing"
                : job->stage == 1              ? "rendering"
                : job->stage == 2              ? "measuring"
                                               : "validating";
        return;
    }
    receipt = std::move(job->result);
    phase = receipt.at("state");
    if (phase == "completed" && job->binding["purpose"] == "export")
        try
        {
            require(!job->cancel.load() && !job->expired(), "export cancelled or deadline expired before publication");
            require(job->binding["session_token"] == owner.sessionToken() &&
                        job->binding["revision"] == owner.revision &&
                        chainHash() == job->binding["processing_chain_hash"].get<std::string>(),
                    "project changed before export publication");
            const auto& v = receipt.at("file_verification");
            require(job->exportStage.getSize() == v["bytes"] &&
                        job->exportStage.getLastModificationTime().toMilliseconds() == v["modified_ms"],
                    "export stage changed before publication");
            std::filesystem::create_hard_link(job->exportStage.getFullPathName().toStdString(),
                                              job->destination.getFullPathName().toStdString());
            receipt["file_verification"]["status"] = "verified_published";
            receipt["file_verification"]["path"] = job->destination.getFullPathName().toStdString();
            receipt["file_verification"]["publication"] =
                "atomic no-overwrite hard link of the verified bytes; external file creation is not Undo";
        }
        catch (const std::exception& e)
        {
            phase = job->cancel ? "cancelled" : job->expired() ? "expired" : "failed";
            receipt = {{"state", phase},
                       {"error", e.what()},
                       {"binding", job->binding},
                       {"artifact_id", job->binding["artifact_id"]},
                       {"runtime", job->runtime()}};
        }
    const auto releaseBegan = now();
    job.reset();
    receipt["runtime"]["release_message_ms"] = now() - releaseBegan;
    if (phase == "completed")
    {
        receipt["current"] = current(receipt);
        // Measurement metadata is derived evidence, separate from edit facts and
        // Undo. Keep a bounded snapshot reference with the actual saved Edit.
        if (receipt["binding"]["session_token"] == owner.sessionToken())
        {
            auto refs = owner.metadata.getOrCreateChildWithName("ANALYSISARTIFACTS", nullptr);
            refs.removeAllChildren(nullptr);
            juce::ValueTree item("ARTIFACT");
            item.setProperty("id", juce::String{receipt["artifact_id"].get<std::string>()}, nullptr);
            item.setProperty("record", juce::String{receipt.dump()}, nullptr);
            refs.addChild(item, -1, nullptr);
        }
    }
}
void MasterAnalysis::timerCallback()
{
    poll();
}
Json MasterAnalysis::status()
{
    poll();
    auto result = observed(receipt);
    const bool measuredProgress = !job || job->binding["purpose"] != "source";
    return {{"state", phase},
            {"busy", bool(job)},
            {"progress", measuredProgress ? Json(job                    ? job->progress.load()
                                                 : phase == "completed" ? 1.
                                                                        : 0.)
                                          : Json(nullptr)},
            {"progress_available", measuredProgress},
            {"request", job ? job->binding : Json(nullptr)},
            {"receipt", result},
            {"pause",
             {{"user_requested", job && job->userPause.load()},
              {"playback_requested", job && job->pause.load()},
              {"worker_parked", job && job->parked.load()},
              {"deadline_includes_pause", true}}},
            {"runtime", job                  ? job->runtime()
                        : result.is_object() ? result.value("runtime", Json(nullptr))
                                             : Json(nullptr)},
            {"budgets",
             {{"workers", 1},
              {"deadline_seconds", 60},
              {"range_seconds", 300},
              {"events", 128},
              {"source_mapping_views", 128}}}};
}
Json MasterAnalysis::control(const std::string& command, const Json& args, const std::string& actor)
{
    const auto requestBegan = now();
    owner.checkThread();
    poll();
    if (command == "status")
    {
        fields(args, {});
        return status();
    }
    if (command == "cancel")
    {
        fields(args, {"artifact_id"});
        require(job && args.at("artifact_id") == job->binding["artifact_id"], "analysis job not pending");
        require(actor == "human" || actor == job->actor, "analysis cancellation belongs to another client");
        job->cancel = true;
        return status();
    }
    if (command == "pause")
    {
        fields(args, {"artifact_id", "paused"});
        require(job && args.at("artifact_id") == job->binding["artifact_id"], "analysis job not pending");
        require(actor == "human" || actor == job->actor, "analysis pause belongs to another client");
        require(args.at("paused").is_boolean(), "analysis paused must be boolean");
        require(!job->cancel.load(), "analysis cancellation already requested");
        job->userPause = args.at("paused").get<bool>();
        return status();
    }
    if (command == "locate_loudness")
    {
        fields(args, {"artifact_id", "point_index", "series", "clip_id", "base_revision"});
        require(actor == "human", "loudness location is a local GUI control");
        require(receipt.is_object() && args.at("artifact_id") == receipt.at("artifact_id"),
                "analysis artifact unavailable");
        bool valid = false;
        try
        {
            valid = current(receipt, true);
        }
        catch (...)
        {
            receipt["invalidated"] = true;
            throw;
        }
        if (!valid)
            receipt["invalidated"] = true;
        require(valid, "analysis is stale; measure the current project before locating");
        require(args.at("series").is_string(), "loudness series required");
        auto point =
            analysis::loudnessPoint(receipt.at("loudness_curve"), integer(args.at("point_index")), args.at("series"));
        require(point["status"] != "insufficient_window", "loudness window is incomplete");
        if (receipt["binding"]["purpose"] == "source")
        {
            require(integer(args.at("base_revision")) == int64_t(owner.revision),
                    "clip mapping revision conflict; refresh location preview");
            const auto frame = point.at("source_location_frame").get<int64_t>();
            const Json event = {{"id", point["point_index"]},
                                {"kind", "loudness_point"},
                                {"source_start_frame", frame},
                                {"source_end_frame", frame + 1}};
            Json mapped = nullptr;
            const auto views = sourceMappings(receipt);
            for (const auto& mapping : views["clips"])
                if (mapping["clip_id"] == args.at("clip_id"))
                    mapped = analysis::projectSourceEvent(event, mapping);
            require(mapped.is_object(), "loudness point is not visible in a supported current clip mapping");
            point["location"] = mapped;
            owner.seek(mapped["start_samples"]);
        }
        else
        {
            require(!args.contains("clip_id") && !args.contains("base_revision"),
                    "processed location does not take source mapping fields");
            owner.seek(point.at("location_samples"));
        }
        return {{"state", "located"}, {"point", point}, {"artifact_id", receipt["artifact_id"]}};
    }
    if (command == "locate")
    {
        fields(args, {"artifact_id", "event_id", "clip_id", "base_revision"});
        require(actor == "human", "analysis location is a local GUI control");
        require(receipt.is_object() && args.at("artifact_id") == receipt.at("artifact_id"),
                "analysis artifact unavailable");
        bool valid = false;
        try
        {
            valid = current(receipt, true);
        }
        catch (...)
        {
            receipt["invalidated"] = true;
            throw;
        }
        if (!valid)
            receipt["invalidated"] = true;
        require(valid, "analysis is stale; measure the current project before locating");
        const auto index = integer(args.at("event_id"));
        require(index < int64_t(receipt.at("events").size()), "analysis event unavailable");
        auto event = receipt["events"][size_t(index)];
        if (receipt["binding"]["purpose"] == "source")
        {
            require(integer(args.at("base_revision")) == int64_t(owner.revision),
                    "clip mapping revision conflict; refresh location preview");
            Json mapped = nullptr;
            const auto mappingSet = sourceMappings(receipt);
            for (const auto& mapping : mappingSet["clips"])
                if (mapping["clip_id"] == args.at("clip_id"))
                    mapped = analysis::projectSourceEvent(event, mapping);
            require(mapped.is_object(), "source event is not visible in a supported current clip mapping");
            event["location"] = mapped;
            owner.seek(mapped["start_samples"]);
        }
        else
        {
            require(!args.contains("clip_id") && !args.contains("base_revision"),
                    "processed location does not take source mapping fields");
            owner.seek(event["start_samples"]);
        }
        return {{"state", "located"}, {"event", event}, {"artifact_id", receipt["artifact_id"]}};
    }
    require(command == "export" || command == "master" || command == "delivery" || command == "source" ||
                command == "track" || command == "clip",
            "unknown analysis control");
    if (command == "export")
        fields(args, {"session_token", "base_revision", "mode", "start_samples", "end_samples", "postroll_samples",
                      "destination", "request_key", "profile"});
    else if (command == "source")
        fields(args, {"session_token", "base_revision", "clip", "source_start_frame", "source_end_frame", "request_key",
                      "profile"});
    else if (command == "clip")
        fields(args, {"session_token", "base_revision", "clip", "start_samples", "end_samples", "request_key",
                      "detector_profile"});
    else if (command == "track")
        fields(args, {"session_token", "base_revision", "track", "tap_point", "start_samples", "end_samples",
                      "request_key", "detector_profile"});
    else if (command == "delivery")
        fields(args, {"session_token", "base_revision", "start_samples", "end_samples", "request_key", "profile",
                      "detector_profile"});
    else
        fields(args,
               {"session_token", "base_revision", "start_samples", "end_samples", "request_key", "detector_profile"});
    require(args.at("session_token") == owner.sessionToken() &&
                integer(args.at("base_revision")) == int64_t(owner.revision),
            "analysis version conflict");
    require(args.at("request_key").is_string() && !args["request_key"].get<std::string>().empty() &&
                args["request_key"].get<std::string>().size() <= 128,
            "analysis request_key required, maximum 128 bytes");
    const bool exporting = command == "export", raw = command == "source", trackTap = command == "track",
               clipTap = command == "clip";
    Json sourceFacts = nullptr;
    te::WaveAudioClip* sourceClip = nullptr;
    std::string tapPoint = clipTap ? "clip_post_fx" : "master", trackID;
    if (trackTap)
    {
        require(args.at("track").is_string() && args.at("tap_point").is_string(),
                "track tap requires actual track ID and registered tap_point");
        trackID = args.at("track");
        tapPoint = args.at("tap_point");
        require(owner.track(trackID), "analysis audio/instrument/Aux track not found");
        require(tapPoint == "track_pre_inserts" || tapPoint == "track_post_inserts" || tapPoint == "bus",
                "unsupported track tap_point");
    }
    const auto start = integer(args.at(raw ? "source_start_frame" : "start_samples")),
               end = integer(args.at(raw ? "source_end_frame" : "end_samples"));
    if (raw)
    {
        require(args.at("clip").is_string(), "source clip ID required");
        sourceClip = owner.audioClip(args["clip"]);
        require(sourceClip != nullptr, "source audio clip not found");
        sourceFacts = owner.audioClipQuery(*sourceClip);
        const double rate = sourceFacts["source_sample_rate"];
        require(rate >= 8000 && rate <= 192000 && end > start && end <= sourceFacts["source_frames"].get<int64_t>() &&
                    end - start <= rate * 300,
                "source range outside media or 300 second budget");
    }
    else
        require(end > start && end - start <= 300 * 48000 && end <= std::llround(te::Edit::maximumLength * 48000),
                "analysis range must be positive and at most 300 seconds");
    if (clipTap)
    {
        require(args.at("clip").is_string(), "clip tap requires actual audio clip ID");
        sourceClip = owner.audioClip(args["clip"]);
        require(sourceClip != nullptr, "Clip FX audio clip not found");
        sourceFacts = owner.audioClipQuery(*sourceClip);
        require(sourceFacts["editable_audio"].get<bool>() && !sourceClip->effectsEnabled(),
                "Clip FX tap of looped/grouped/stretched/warped/reversed or offline ClipEffects media is not qualified "
                "yet");
        const auto p = sourceClip->getPosition();
        require(start >= std::llround(p.getStart().inSeconds() * 48000) &&
                    end <= std::llround(p.getEnd().inSeconds() * 48000),
                "Clip FX range must be inside the actual clip");
        for (auto* plugin : *sourceClip->getPluginList())
            require(dynamic_cast<te::EqualiserPlugin*>(plugin) || dynamic_cast<te::CompressorPlugin*>(plugin) ||
                        dynamic_cast<te::ReverbPlugin*>(plugin) || dynamic_cast<te::DelayPlugin*>(plugin),
                    "Clip FX tap contains an unqualified plugin type");
    }
    int64_t postroll = 0;
    juce::File destination;
    if (exporting)
    {
        require(actor == "human", "verified export requires a local GUI file grant");
        require(args.at("mode") == "session" || args.at("mode") == "selection", "invalid export range mode");
        const auto bound = owner.exportRequest(args.at("mode") == "selection");
        for (const auto& key : {"session_token", "base_revision", "mode", "start_samples", "end_samples"})
            require(bound.at(key) == args.at(key), "export range changed; prepare a fresh preview");
        postroll = integer(args.at("postroll_samples"));
        require(postroll <= 30 * 48000 && end - start + postroll + 96000 <= 300 * 48000 &&
                    end + postroll + 96000 <= std::llround(te::Edit::maximumLength * 48000),
                "verified export including postroll and two-second continuation exceeds budget");
        require(args.at("destination").is_string() &&
                    juce::File::isAbsolutePath(juce::String{args["destination"].get<std::string>()}),
                "export destination must be an absolute local WAV path");
        destination = juce::File(juce::String{args["destination"].get<std::string>()});
        require(destination.hasFileExtension("wav") && destination.getParentDirectory().isDirectory(),
                "export requires WAV in an existing local directory");
    }
    const auto profile = raw ? analysis::sourceProfile(args.value("profile", Json::object()))
                         : command == "delivery" || exporting
                             ? delivery::normaliseProfile(args.value("profile", Json::object()))
                             : Json(nullptr);
    const auto renderProfile = !raw && args.contains("detector_profile")
                                   ? analysis::sourceProfile(args.at("detector_profile"))
                                   : Json(nullptr);
    const auto renderProfileText = renderProfile.dump();
    const auto intent =
        Json{{"session_token", owner.sessionToken()},
             {"revision", owner.revision},
             {"begin", start},
             {"end", end},
             {"clip", raw || clipTap ? args.at("clip") : Json(nullptr)},
             {"track", trackTap ? Json(trackID) : Json(nullptr)},
             {"tap_point", trackTap || clipTap ? Json(tapPoint) : Json(nullptr)},
             {"purpose", command},
             {"profile", profile},
             {"detector_profile", renderProfile},
             {"postroll_samples", postroll},
             {"destination", exporting ? Json(destination.getFullPathName().toStdString()) : Json(nullptr)}}
            .dump();
    const auto intentHash = juce::SHA256(intent.data(), intent.size()).toHexString().toStdString();
    if (job)
    {
        require(job->actor == actor && job->binding["request_key"] == args["request_key"] &&
                    job->binding["request_fingerprint"] == intentHash,
                "one analysis job is already running or retry differs from its original intent");
        return status();
    }
    if (receipt.is_object() && receipt["binding"]["request_key"] == args["request_key"])
    {
        require(receipt["binding"]["actor"] == actor && receipt["binding"]["revision"] == owner.revision &&
                    receipt["binding"]["request_fingerprint"] == intentHash,
                "analysis key reused with different intent");
        return status();
    }
    if (exporting)
        require(!destination.exists(), "export destination already exists; choose a new file");
    require(!owner.edit->getTransport().isPlaying() && owner.recordingCapture.is_null() && owner.capture.is_null() &&
                owner.parameterCapture.is_null() && !owner.audioConfigurationPending(),
            "stop transport and finish gestures before preparing analysis");
    if (raw)
    {
        auto work = std::make_unique<Job>();
        work->actor = actor;
        work->started = requestBegan;
        work->pcm = sourceClip->getOriginalFile();
        require(work->pcm.existsAsFile(), "source media missing");
        work->binding = {{"artifact_id", juce::Uuid().toString().toStdString()},
                         {"request_key", args["request_key"]},
                         {"actor", actor},
                         {"session_token", owner.sessionToken()},
                         {"revision", owner.revision},
                         {"purpose", command},
                         {"request_fingerprint", intentHash},
                         {"tap_point", "source_clip"},
                         {"object_id", args["clip"]},
                         {"source_start_frame", start},
                         {"source_end_frame", end},
                         {"source_sample_rate", sourceFacts["source_sample_rate"]},
                         {"position_units", "native source-file frames"},
                         {"detector_profile", profile},
                         {"created_utc", juce::Time::getCurrentTime().toISO8601(true).toStdString()}};
        work->sources = Json::array({{{"clip_id", args["clip"]},
                                      {"path", work->pcm.getFullPathName().toStdString()},
                                      {"bytes", work->pcm.getSize()},
                                      {"modified_ms", work->pcm.getLastModificationTime().toMilliseconds()}}});
        work->preparation["total"] = now() - requestBegan;
        receipt = nullptr;
        job = std::move(work);
        job->launch();
        phase = "hashing";
        return status();
    }
    owner.captureNativeStates();
    require(!owner.nativeStates || (!owner.nativeStates->query()["pending"].get<bool>() &&
                                    owner.nativeStates->query()["failure"].is_null()),
            "resolve native plugin state before analysis");
    if (!clipTap)
        owner.validateExternalRuntime();
    auto captured = owner.recoverySnapshot();
    auto work = std::make_unique<Job>();
    work->actor = actor;
    work->started = requestBegan;
    work->preparation["snapshot_capture"] = captured.second["snapshot_capture_ms"];
    auto preparationSpan = now();
    const auto processingHash = chainHash();
    work->preparation["chain_hash"] = now() - preparationSpan;
    work->binding = {
        {"artifact_id", juce::Uuid().toString().toStdString()},
        {"request_key", args["request_key"]},
        {"actor", actor},
        {"session_token", owner.sessionToken()},
        {"revision", owner.revision},
        {"purpose", command},
        {"request_fingerprint", intentHash},
        {"delivery_profile", profile},
        {"detector_profile", renderProfile},
        {"detector_profile_sha256",
         renderProfile.is_object()
             ? Json(juce::SHA256(renderProfileText.data(), renderProfileText.size()).toHexString().toStdString())
             : Json(nullptr)},
        {"tap_point", tapPoint},
        {"object_id", clipTap    ? args["clip"].get<std::string>()
                      : trackTap ? trackID
                                 : "master"},
        {"start_samples", start},
        {"end_samples", end},
        {"timeline_sample_rate", 48000},
        {"processing_chain_hash", processingHash},
        {"chain_hash_scope", "entire committed Edit; known VolumeAndPan/EQ/Delay curve-driven caches normalized; "
                             "conservative invalidation"},
        {"created_utc", juce::Time::getCurrentTime().toISO8601(true).toStdString()}};
    work->sources = Json::array();
    for (auto* track : te::getAudioTracks(*owner.edit))
        for (auto* clip : track->getClips())
            if (auto* audio = dynamic_cast<te::WaveAudioClip*>(clip); audio && (!clipTap || audio == sourceClip))
            {
                auto facts = owner.audioClipQuery(*audio);
                const std::string path = facts.at("path");
                const juce::File file(juce::String{path});
                require(file.existsAsFile(), "analysis source media missing");
                work->sources.push_back({{"clip_id", clip->itemID.toString().toStdString()},
                                         {"track_id", track->itemID.toString().toStdString()},
                                         {"path", path},
                                         {"bytes", file.getSize()},
                                         {"modified_ms", file.getLastModificationTime().toMilliseconds()}});
            }
    require(work->sources.size() <= 4096, "analysis source count exceeds current 4096-clip budget");
    work->sources = mediaManifest(work->sources);
    if (clipTap)
    {
        work->binding["tap_configuration"] = isolateClipState(captured.first, *sourceClip);
        const auto descriptor = work->binding["tap_configuration"].dump();
        work->binding["tap_configuration_sha256"] =
            juce::SHA256(descriptor.data(), descriptor.size()).toHexString().toStdString();
    }
    if (exporting)
    {
        work->destination = destination;
        work->binding["export_range"] = {{"mode", args["mode"]},
                                         {"session_token", args["session_token"]},
                                         {"base_revision", args["base_revision"]},
                                         {"start_samples", start},
                                         {"end_samples", end}};
        work->binding["postroll_samples"] = postroll;
        work->binding["end_samples"] = end + postroll;
        work->binding["continuation_samples"] = 96000;
        work->binding["tap_point"] = "exported_master_file";
        work->binding["destination"] = destination.getFullPathName().toStdString();
    }
    const auto editFile = owner.edit->editFileRetriever ? owner.edit->editFileRetriever() : juce::File{};
    te::Edit::Options options{owner.engine, std::move(captured.first), owner.edit->getProjectItemRef()};
    options.role = te::Edit::forRendering;
    options.numAudioTracks = 0;
    options.editFileRetriever = [editFile] { return editFile; };
    preparationSpan = now();
    work->snapshot = te::Edit::createEdit(std::move(options));
    require(work->snapshot != nullptr, "analysis snapshot creation failed");
    work->preparation["edit_create"] = now() - preparationSpan;
    preparationSpan = now();
    if (clipTap)
    {
        const auto tracks = te::getAudioTracks(*work->snapshot);
        require(tracks.size() == 1 && tracks[0]->getClips().size() == 1,
                "isolated clip snapshot contains unexpected playback objects");
        for (auto* p : tracks[0]->pluginList.getPlugins())
            p->deleteFromParent();
        tracks[0]->setMute(false);
        tracks[0]->setSolo(false);
        tracks[0]->setSoloIsolate(true);
        tracks[0]->getOutput().setOutputToDefaultDevice(false);
    }
    if (trackTap)
    {
        work->binding["tap_configuration"] = prepareTrackTap(*work->snapshot, trackID, tapPoint);
        const auto descriptor = work->binding["tap_configuration"].dump();
        work->binding["tap_configuration_sha256"] =
            juce::SHA256(descriptor.data(), descriptor.size()).toHexString().toStdString();
    }
    work->preparation["tap_prepare"] = now() - preparationSpan;
    if (exporting)
    {
        const auto own = destination.getParentDirectory().getChildFile(".forma-export-" + juce::Uuid().toString());
        require(std::filesystem::create_directory(own.getFullPathName().toStdString()),
                "private export stage could not be created");
        work->directory = own;
        work->exportStage = own.getChildFile("verified.wav");
    }
    else
    {
        work->directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getNonexistentChildFile("forma-analysis", {}, false);
        require(work->directory.createDirectory().wasOk(), "analysis temporary storage unavailable");
    }
    work->pcm = work->directory.getChildFile("master-float.wav");
    work->renderStatus = std::make_unique<te::Edit::ScopedRenderStatus>(*work->snapshot, false);
    te::Renderer::Parameters p(*work->snapshot);
    p.destFile = work->pcm;
    p.audioFormat = &work->wav;
    p.bitDepth = 32;
    p.sampleRateForAudio = 48000;
    p.canRenderInMono = false;
    p.useMasterPlugins = !trackTap && !clipTap;
    p.tracksToDo = te::toBitSet(te::getAllTracks(*work->snapshot));
    p.time = {tracktion::TimePosition::fromSeconds(start / 48000.),
              tracktion::TimePosition::fromSeconds((end + (exporting ? postroll + 96000 : 0)) / 48000.)};
    preparationSpan = now();
    work->task = std::make_unique<te::Renderer::RenderTask>(clipTap    ? "Forma Clip FX analysis"
                                                            : trackTap ? "Forma track tap analysis"
                                                                       : "Forma Master analysis",
                                                            p, &work->progress, nullptr);
    work->preparation["graph_build"] = now() - preparationSpan;
    work->preparation["total"] = now() - requestBegan;
    auto* flag = work.get();
    work->task->setCancellationCheck([flag] { return flag->cancel.load() || flag->expired(); });
    receipt = nullptr;
    job = std::move(work);
    job->launch();
    phase = "hashing";
    return status();
}
Json Commands::analysisControl(const std::string& cmd, const Json& args, const std::string& actor)
{
    checkThread();
    if (!masterAnalysis)
        masterAnalysis = std::make_unique<MasterAnalysis>(*this);
    return masterAnalysis->control(cmd, args, actor);
}
Json Commands::analysisStatus()
{
    checkThread();
    if (!masterAnalysis)
        return {{"state", "idle"}, {"busy", false}, {"receipt", nullptr}};
    return masterAnalysis->status();
}
void Commands::registerAnalysisCommands(Json& registry)
{
    const Json string{{"type", "string"}, {"minLength", 1}, {"maxLength", 128}},
        integer{{"type", "integer"}, {"minimum", 0}, {"maximum", int64_t(9007199254740991)}};
    auto add = [&](const char* id, const char* tool, const char* method, const char* description, Json properties,
                   Json required)
    {
        registry.push_back({{"id", id},
                            {"execution", "analysis"},
                            {"tool_name", tool},
                            {"queue_method", method},
                            {"description", description},
                            {"permission", "read"},
                            {"risk", "none"},
                            {"reversible", false},
                            {"schema",
                             {{"type", "object"},
                              {"properties", properties},
                              {"required", required},
                              {"additionalProperties", false}}},
                            {"test", "M3-MASTER-01"}});
    };
    registry.push_back({{"id", "export.verified"},
                        {"execution", "control"},
                        {"actor", "human"},
                        {"permission", "local_gui"},
                        {"risk", "external_file_write"},
                        {"reversible", false},
                        {"test", "M3-EXPORT-01"},
                        {"description", "Local file grant; asynchronous verified PCM24 WAV with explicit session "
                                        "postroll and two-second continuation; atomic no-overwrite publication. Query "
                                        "analysis.status for the actual receipt; no external write MCP tool."},
                        {"schema",
                         {{"type", "object"},
                          {"properties",
                           {{"session_token", string},
                            {"base_revision", integer},
                            {"mode", {{"type", "string"}, {"enum", {"session", "selection"}}}},
                            {"start_samples", integer},
                            {"end_samples", integer},
                            {"postroll_samples", {{"type", "integer"}, {"minimum", 0}, {"maximum", 1440000}}},
                            {"destination", {{"type", "string"}, {"minLength", 1}}},
                            {"request_key", string},
                            {"profile", delivery::profileSchema()}}},
                          {"required",
                           {"session_token", "base_revision", "mode", "start_samples", "end_samples",
                            "postroll_samples", "destination", "request_key"}},
                          {"additionalProperties", false}}}});
    add("analysis.master", "analyze_master", "analysis_master",
        "Prepare a stopped Edit snapshot and asynchronously measure its Master range. Local-only float32 render; no "
        "upload or edit. Poll query_analysis for the actual receipt. Positions are half-open session samples at 48 "
        "kHz, maximum 300 seconds; a live request_key retry must be identical.",
        {{"session_token", string},
         {"base_revision", integer},
         {"start_samples", integer},
         {"end_samples", integer},
         {"request_key", string}},
        Json::array({"session_token", "base_revision", "start_samples", "end_samples", "request_key"}));
    add("analysis.delivery", "analyze_delivery", "analysis_delivery",
        "Asynchronously measure the actual stopped Master range and evaluate explicit LUFS-I/True "
        "Peak/full-scale/ending-level criteria. This is local evidence, not an export, platform certification or proof "
        "of complete effect tails. Poll query_analysis; a completed measurement may have failed/indeterminate/review "
        "criteria. Example defaults are -14 LUFS +/-1 and -1 dBTP; profile is configurable. Retry the same key with "
        "identical purpose/range/profile.",
        {{"session_token", string},
         {"base_revision", integer},
         {"start_samples", integer},
         {"end_samples", integer},
         {"request_key", string},
         {"profile", delivery::profileSchema()}},
        Json::array({"session_token", "base_revision", "start_samples", "end_samples", "request_key"}));
    registry.back()["test"] = "M3-DELIVERY-01";
    add("analysis.source", "analyze_source_clip", "analysis_source",
        "Decode an actual audio clip's original local media without clip FX/gain, track inserts or Master processing. "
        "Positions are native source-file frames, NOT session samples. Measure raw PCM and detect contiguous low-level "
        "intervals and estimated short-time energy-rise candidates. These are not breath or performance-quality "
        "judgements. Poll query_analysis for actual evidence and CURRENT clip mappings; move/trim/split do not shift "
        "the stored source frames. Loop/auto-tempo/warp/reverse mappings are explicitly unavailable. Maximum 300 "
        "seconds; no arbitrary path or upload.",
        {{"session_token", string},
         {"base_revision", integer},
         {"clip", string},
         {"source_start_frame", integer},
         {"source_end_frame", integer},
         {"request_key", string},
         {"profile", analysis::sourceProfileSchema()}},
        Json::array(
            {"session_token", "base_revision", "clip", "source_start_frame", "source_end_frame", "request_key"}));
    registry.back()["test"] = "M3-SOURCE-01";
    add("analysis.track", "analyze_track", "analysis_track",
        "Asynchronously measure a real audio/instrument/Aux track boundary through native Tracktion sends in a "
        "render-only snapshot. track_pre_inserts is after clip FX/input sum and instruments/Aux returns but before "
        "track effects/fader; track_post_inserts is after effects before VolumeAndPan; bus is the end of the track "
        "chain, including fader/pan. Preserve upstream routes, sends, sidechains and original mute/solo semantics; "
        "exclude Master and other direct outputs. Half-open session samples at 48 kHz, maximum 300 seconds; actual "
        "track ID required. One worker, no upload or active-Edit change. Poll query_analysis; processing revisions "
        "invalidate evidence.",
        {{"session_token", string},
         {"base_revision", integer},
         {"track", string},
         {"tap_point", {{"type", "string"}, {"enum", {"track_pre_inserts", "track_post_inserts", "bus"}}}},
         {"start_samples", integer},
         {"end_samples", integer},
         {"request_key", string}},
        Json::array(
            {"session_token", "base_revision", "track", "tap_point", "start_samples", "end_samples", "request_key"}));
    registry.back()["test"] = "M3-TAP-01";
    add("analysis.clip", "analyze_clip", "analysis_clip",
        "Render only an actual audio clip after its native clip gain/pan, EQ/Compressor/Reverb/Delay plugin chain and "
        "fades. Exclude other clips, track effects/fader/mute/solo/VCA, routed inputs/sends and Master. The active "
        "Edit is unchanged. Range is half-open session samples at 48 kHz INSIDE this clip, NOT native source frames. "
        "Linear ungrouped/unwarped/non-looping/non-reversed/un-stretched media only; offline ClipEffects and "
        "third-party clip chains are not qualified and reject. One local worker/300 seconds; poll query_analysis for "
        "actual current receipt. Processing edits and Undo invalidate it; never invent a result from the raw source "
        "tap.",
        {{"session_token", string},
         {"base_revision", integer},
         {"clip", string},
         {"start_samples", integer},
         {"end_samples", integer},
         {"request_key", string}},
        Json::array({"session_token", "base_revision", "clip", "start_samples", "end_samples", "request_key"}));
    registry.back()["test"] = "M3-CLIPFX-01";
    for (auto& entry : registry)
        if (entry["id"] == "analysis.master" || entry["id"] == "analysis.track" || entry["id"] == "analysis.delivery" ||
            entry["id"] == "analysis.clip")
        {
            entry["schema"]["properties"]["detector_profile"] = analysis::sourceProfileSchema();
            entry["description"] =
                entry["description"].get<std::string>() +
                " Optional detector_profile enables measured silence intervals and estimated energy-rise candidates "
                "from this rendered tap, with session-sample coordinates and a condition hash; omission means NOT "
                "analysed, not absent. Same-key retries must retain the normalized profile. Candidates do not identify "
                "breaths or performance quality.";
            entry["additional_tests"] = Json::array({"M3-EVENTS-01"});
        }
    add("analysis.status", "query_analysis", "analysis_status",
        "Read actual analysis progress, provenance and bounded events. Raw source artifacts retain native file frames "
        "across move/trim/split and gain/insert edits, with CURRENT mapping_revision and clip views; Processed "
        "Master/track/clip artifacts invalidate on processing changes. Optional processed events are session samples, "
        "with condition hashes and explicit detection enablement; a transient is an estimate, not quality or breath "
        "recognition. current=false means stale or transport is playing; never use stale events for an edit. Source "
        "pending progress is unavailable, not a fabricated percentage. Full-scale exceedance is clipping risk, not "
        "proof of original damage.",
        Json::object(), Json::array());
    for (auto& entry : registry)
        if (entry.value("execution", std::string{}) == "analysis" && entry["id"] != "analysis.cancel")
        {
            entry["description"] =
                entry["description"].get<std::string>() +
                " Completed evidence includes loudness_curve columns [decoded_end_frame, momentary_lufs, "
                "short_term_lufs] on a complete 100 ms grid after the first 400 ms window; 400 ms M / 3 s S, 1e-6 LU "
                "resolution, at most 3000 points. Null is negative infinity once a full window exists, otherwise "
                "insufficient_window. Ends are exclusive relative decoded frames; raw curves use native source frames, "
                "processed curves session samples via declared origin/rate. No interpolation is measured evidence; no "
                "live meter or complete-tail qualification.";
            auto& tests = entry["additional_tests"];
            if (!tests.is_array())
                tests = Json::array();
            tests.push_back("M3-LUFS-01");
            tests.push_back("M3-SPECTRUM-01");
            entry["description"] =
                entry["description"].get<std::string>() +
                " Completed spectrum contains all 2049 one-sided 4096-frame periodic-Hann bin powers (2048-frame hop), "
                "equal-weight averaged across actual complete windows and separately transformed channels. A distinct "
                "end-aligned full window covers the tail; short ranges are insufficient_window with no padding. Band "
                "powers partition bin centres; DC/Nyquist are included. This is window-weighted mean-square power, not "
                "unwindowed RMS, PSD/Hz, a timestamped event or timbre judgement. Raw and processed "
                "provenance/invalidation match the parent artifact.";
        }
    for (auto& entry : registry)
        if (entry["id"] == "analysis.status")
        {
            entry["description"] =
                entry["description"].get<std::string>() +
                " runtime reports completed wall-clock phase durations (including pause/SDK/I/O waits), actual source "
                "hash reads/bytes and message-thread release; unfinished phase times remain zero. Processed media is "
                "unique by exact path with clip_references retaining every actual clip/track ID, first-reference IDs "
                "remain aliases. Each before/after/deep-locate hash pass is fresh, never a cross-pass mtime cache. "
                "Pause separates request/worker acknowledgement and the deadline includes parking.";
            entry["additional_tests"].push_back("M3-RESOURCES-01");
        }
    add("analysis.cancel", "cancel_analysis", "analysis_cancel",
        "Cancel your own pending analysis by actual artifact_id. Await the terminal cancelled receipt; cancellation "
        "cannot make an analysis successful.",
        {{"artifact_id", string}}, Json::array({"artifact_id"}));
    add("analysis.pause", "set_analysis_paused", "analysis_pause",
        "Request pause/resume of your own pending local analysis by artifact_id. paused must be boolean. "
        "query_analysis distinguishes user_requested/playback_requested from worker_parked; pausing is not an "
        "acknowledgement. Resuming cannot override playback/audio-device priority. The 60 second deadline includes "
        "pause; nonpreemptible SDK/plugin/I/O calls may delay acknowledgement. No Edit/Undo change.",
        {{"artifact_id", string}, {"paused", {{"type", "boolean"}}}}, Json::array({"artifact_id", "paused"}));
    registry.back()["test"] = "M3-RESOURCES-01";
}
} // namespace ndaw::v2
