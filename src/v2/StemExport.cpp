// SPDX-License-Identifier: AGPL-3.0-only
#include "StemExport.h"
#include "CheckedMedia.h"
#include "MasterAnalysis.h"
#include <nativedaw/v2/ExportVerification.h>
#include <nativedaw/v2/DeliveryCheck.h>
#include <filesystem>
#include <cerrno>
#if JUCE_MAC
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#elif JUCE_LINUX
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/fs.h>
#elif JUCE_WINDOWS
#include <windows.h>
#endif
namespace ndaw::v2
{
namespace
{
void require(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
}
std::string id(te::Track& t)
{
    return t.itemID.toString().toStdString();
}
Json sourceFiles(te::Edit& edit)
{
    std::map<std::string, juce::File> files;
    for (auto* t : te::getAudioTracks(edit))
        if (t->isFrozen(te::Track::individualFreeze))
        {
            const auto f = te::TemporaryFileManager::getFreezeFileForTrack(*t);
            files[f.getFullPathName().toStdString()] = f;
        }
        else
            for (auto* c : t->getClips())
                if (auto* w = dynamic_cast<te::WaveAudioClip*>(c))
                {
                    const auto f = w->getOriginalFile();
                    files[f.getFullPathName().toStdString()] = f;
                }
    Json out = Json::array();
    for (const auto& [path, f] : files)
    {
        require(f.existsAsFile(), "Stems依赖的源文件缺失");
        out.push_back(
            {{"path", path}, {"bytes", f.getSize()}, {"modified_ms", f.getLastModificationTime().toMilliseconds()}});
    }
    return out;
}
Json hashes(Json files, const std::function<void()>& check = {})
{
    for (auto& f : files)
    {
        const juce::File file(juce::String(f["path"].get<std::string>()));
        require(file.existsAsFile() && file.getSize() == f["bytes"].get<int64_t>() &&
                    file.getLastModificationTime().toMilliseconds() == f["modified_ms"].get<int64_t>(),
                "导出期间源媒体改变");
        f["sha256"] = checkedMediaHash(file, check);
    }
    return files;
}
juce::String safeName(juce::String s)
{
    juce::String out;
    for (auto p = s.getCharPointer(); !p.isEmpty();)
    {
        const auto c = p.getAndAdvance();
        out += c < 32 || c == '/' || c == '\\' || c == ':' || c == '?' || c == '*' || c == '<' || c == '>' ||
                       c == '|' || c == '"'
                   ? juce::juce_wchar('_')
                   : c;
    }
    out = out.trim().substring(0, 64);
    while (out.getNumBytesAsUTF8() > 180)
        out = out.dropLastCharacters(1);
    return out.isEmpty() ? juce::String("Track") : out;
}
void captureOutput(te::Edit& edit, const std::string& targetID)
{
    te::AudioTrack* target = nullptr;
    std::set<int> used;
    for (auto* t : te::getAudioTracks(edit))
    {
        if (id(*t) == targetID)
            target = t;
        for (auto* p : t->pluginList)
        {
            if (auto* s = dynamic_cast<te::AuxSendPlugin*>(p))
                used.insert(s->getBusNumber());
            if (auto* r = dynamic_cast<te::AuxReturnPlugin*>(p))
                used.insert(r->busNumber.get());
        }
    }
    require(target, "Stems轨道失效");
    // Native Aux sends intentionally keep feeding returns when their source is
    // muted. A stem represents the audible main output, not that send branch.
    // Capture audibility before removing device routes (destination mute/solo
    // inheritance is part of the original session's output policy).
    edit.updateMuteSoloStatuses();
    const bool audible = target->shouldBePlayed();
    int bus = 0;
    while (used.contains(bus))
    {
        require(bus < std::numeric_limits<int>::max(), "离线捕获总线耗尽");
        ++bus;
    }
    require(target->pluginList.size() < edit.engine.getEngineBehaviour().getEditLimits().maxPluginsOnTrack,
            "离线捕获需要一个临时插件槽");
    auto capture = edit.insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(edit), nullptr, false);
    require(capture, "Stems捕获轨创建失败");
    capture->setSoloIsolate(true);
    capture->setMute(!audible);
    auto ret = edit.getPluginCache().createNewPlugin(te::AuxReturnPlugin::xmlTypeName, {});
    auto sent = edit.getPluginCache().createNewPlugin(te::AuxSendPlugin::xmlTypeName, {});
    require(ret && sent, "Stems捕获节点创建失败");
    dynamic_cast<te::AuxReturnPlugin&>(*ret).busNumber = bus;
    auto& send = dynamic_cast<te::AuxSendPlugin&>(*sent);
    send.busNumber = bus;
    send.gain->setParameter(te::decibelsToVolumeFaderPosition(0), juce::dontSendNotification);
    capture->pluginList.insertPlugin(ret, 0, nullptr);
    target->pluginList.insertPlugin(sent, target->pluginList.size(), nullptr);
    for (auto* t : te::getAllTracks(edit))
        if (t != capture.get())
            if (auto* out = te::getTrackOutput(*t); out && !out->getDestinationTrack())
                out->setOutputToNone();
    capture->getOutput().setOutputToDefaultDevice(false);
}
void exclusiveRename(const juce::File& from, const juce::File& to)
{
#if JUCE_MAC
    const bool ok = renameatx_np(AT_FDCWD, from.getFullPathName().toRawUTF8(), AT_FDCWD,
                                 to.getFullPathName().toRawUTF8(), RENAME_EXCL) == 0;
#elif JUCE_LINUX
    const bool ok = syscall(SYS_renameat2, AT_FDCWD, from.getFullPathName().toRawUTF8(), AT_FDCWD,
                            to.getFullPathName().toRawUTF8(), RENAME_NOREPLACE) == 0;
#elif JUCE_WINDOWS
    const bool ok = MoveFileExW(from.getFullPathName().toWideCharPointer(), to.getFullPathName().toWideCharPointer(),
                                MOVEFILE_WRITE_THROUGH) != 0;
#else
    const bool ok = false;
#endif
    require(ok, "批次目录冲突或原子发布失败；未覆盖已有目录");
}
Json compact(Json measured)
{
    Json out = Json::object();
    for (const char* k : {"frames", "channels", "sample_rate", "file_bits", "peak_dbfs", "rms_dbfs", "lufs_i",
                          "true_peak_dbtp", "file_verification", "continuation"})
        out[k] = measured.at(k);
    out["delivery_status"] = measured["delivery"]["status"];
    out["float_encoding_risk"] = measured["delivery"]["float_encoding_risk"];
    out["audio_verified"] = true;
    return out;
}
} // namespace
struct StemExport::Batch
{
    juce::ValueTree snapshot;
    juce::File originalEditFile, stage, destination;
    Json binding, items, files = Json::array(), sourceHashes;
    bool ready = false, published = false;
    ~Batch()
    {
        if (!published && stage != juce::File{})
            stage.deleteRecursively();
    }
};
struct StemExport::Job
{
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<te::Edit::ScopedRenderStatus> renderStatus;
    std::unique_ptr<te::Renderer::RenderTask> task;
    juce::WavAudioFormat wav;
    std::atomic<bool> cancel{false}, pause{false}, done{false};
    std::atomic<float> progress{0};
    std::atomic<int> phase{0};
    juce::File raw, encoded;
    Json sourceRefs, result, inputs = Json::array();
    int64_t start = 0, frames = 0;
    double began = juce::Time::getMillisecondCounterHiRes();
    bool expired() const
    {
        return juce::Time::getMillisecondCounterHiRes() - began > 60000;
    }
    ~Job()
    {
        task.reset();
        renderStatus.reset();
        edit.reset();
        raw.deleteFile();
    }
    void run()
    {
        try
        {
            const auto checkpoint = [this]
            {
                while (pause && !cancel && !expired())
                    juce::Thread::sleep(5);
                require(!cancel && !expired(), "Stems取消或单文件超过60秒预算");
            };
            const auto before = hashes(sourceRefs, checkpoint);
            phase = 1;
            while (task->runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
                checkpoint();
            checkpoint();
            require(task->errorMessage.isEmpty(), task->errorMessage.toRawUTF8());
            phase = 2;
            analysis::Control control;
            control.cancelled = [this] { return cancel.load() || expired(); };
            control.yield = checkpoint;
            auto measured = analysis::stageVerifiedWav(raw, encoded, start, frames, control,
                                                       delivery::normaliseProfile(Json::object()));
            phase = 3;
            require(before == hashes(sourceRefs, checkpoint), "Stems源媒体哈希在渲染中改变");
            result = {{"state", "verified"},
                      {"measurement", compact(std::move(measured))},
                      {"sources", before},
                      {"elapsed_ms", juce::Time::getMillisecondCounterHiRes() - began}};
        }
        catch (const std::exception& e)
        {
            result = {{"state", cancel.load() ? "cancelled" : "failed"}, {"error", e.what()}};
        }
    }
};
struct StemExport::Worker
{
    std::thread thread;
    std::optional<te::ScopedThreadExitStatusEnabler> exitEnabler;
    juce::WaitableEvent available;
    std::atomic<Job*> pending{nullptr};
    std::atomic<bool> exiting{false};
    Worker()
    {
        auto gate = std::make_shared<juce::WaitableEvent>();
        thread = std::thread(
            [this, gate]
            {
                gate->wait();
#if JUCE_MAC
                // Utility work remains below interactive/audio priorities without macOS's
                // background disk throttling on every source/verification read.
                pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif
                while (!exiting.load())
                {
                    available.wait();
                    if (exiting.load())
                        break;
                    if (auto* current = pending.load(std::memory_order_acquire))
                    {
                        current->run();
                        pending.store(nullptr, std::memory_order_release);
                        current->done.store(true, std::memory_order_release);
                    }
                }
            });
        exitEnabler.emplace(thread.get_id());
        gate->signal();
    }
    void submit(Job* task)
    {
        require(!pending.load(), "已有Stems工作任务");
        pending.store(task, std::memory_order_release);
        available.signal();
    }
    ~Worker()
    {
        if (auto* current = pending.load())
            current->cancel = true;
        exiting = true;
        te::signalThreadShouldExit(thread.get_id());
        available.signal();
        thread.join();
    }
};
StemExport::StemExport(Commands& c) : owner(c) {}
StemExport::~StemExport()
{
    stopTimer();
    worker.reset();
    job.reset();
    batch.reset();
}
Json StemExport::prepare(const Json& a)
{
    owner.checkThread();
    poll();
    require(!job, "已有Stems任务，请等待或取消");
    require(a.is_object() && a.size() == 10, "Stems参数不完整");
    for (const char* k : {"targets", "include_master", "mode", "start_samples", "end_samples", "tail_samples",
                          "directory", "batch_name", "session_token", "base_revision"})
        require(a.contains(k), "Stems字段缺失");
    require(a["session_token"] == owner.sessionToken() && a["base_revision"] == owner.revision, "Stems界面版本已失效");
    require(a["mode"] == "session" || a["mode"] == "selection", "未知导出范围");
    const auto range = owner.exportRequest(a["mode"] == "selection");
    require(a["start_samples"] == range["start_samples"] && a["end_samples"] == range["end_samples"], "导出范围已变化");
    require(a["include_master"].is_boolean() && a["targets"].is_array() && a["targets"].size() <= 64,
            "请选择最多64个实际音频输出轨道");
    require(!a["targets"].empty() || a["include_master"].get<bool>(), "导出对象为空");
    require(!owner.masterAnalysis || !owner.masterAnalysis->status()["busy"].get<bool>(), "已有分析或导出任务");
    require(!owner.trackPrintStatus()["busy"].get<bool>(), "已有印刻任务");
    require(!owner.playbackPreparation()["busy"].get<bool>(), "弹力音频缓存未就绪");
    owner.validateSidechainRuntime();
    owner.validateFreezeRuntime();
    owner.validateExternalRuntime();
    require(a["tail_samples"].is_number_integer(), "尾音必须是工程采样整数");
    const int64_t tail = a["tail_samples"], last = a["end_samples"];
    require(tail >= 0 && tail <= 120 * 48000 && last <= std::llround(te::Edit::maximumLength * 48000) - tail - 96000,
            "尾音或结束位置超出范围");
    const auto parentPath = juce::String(a["directory"].get<std::string>());
    require(juce::File::isAbsolutePath(parentPath), "导出目录必须是绝对路径");
    const juce::File parent(parentPath);
    require(parent.isDirectory(), "导出父目录不存在");
    const auto name = juce::String(a["batch_name"].get<std::string>());
    require(name.isNotEmpty() && name.getNumBytesAsUTF8() <= 200 && name.trim() == name && name != "." &&
                name != ".." && !name.containsAnyOf("/\\:\r\n"),
            "批次名称不能包含路径或控制字符，最多200字节");
    for (auto p = name.getCharPointer(); !p.isEmpty();)
        require(p.getAndAdvance() >= 32, "批次名称包含控制字符");
    const auto destination = parent.getChildFile(name);
    require(!destination.exists(), "批次目录已存在，请使用新名称");
    Json items = Json::array();
    std::set<std::string> seen;
    int number = 0;
    for (const auto& v : a["targets"])
    {
        require(v.is_string(), "Stems需要稳定轨道ID");
        auto* t = owner.track(v);
        require(t && seen.insert(v.get<std::string>()).second, "轨道不存在或重复");
        const auto kind = t->state.getProperty("ndaw_role", "audio").toString();
        require(kind != "midi" ||
                    std::any_of(t->pluginList.begin(), t->pluginList.end(), [](auto* p) { return p->isSynth(); }),
                "MIDI轨需要实际乐器才能打印音频");
        items.push_back(
            {{"track", v},
             {"name", t->getName().toStdString()},
             {"tap_point", "track_post_fader"},
             {"filename",
              (juce::String(++number).paddedLeft('0', 2) + "-" + safeName(t->getName()) + ".wav").toStdString()},
             {"mute", t->isMuted(false)},
             {"solo", t->isSolo(false)}});
    }
    if (a["include_master"].get<bool>())
        items.push_back({{"track", nullptr},
                         {"name", "Master"},
                         {"tap_point", "master"},
                         {"filename", (juce::String(++number).paddedLeft('0', 2) + "-Master.wav").toStdString()}});
    auto captured = owner.recoverySnapshot();
    require(owner.revision == a["base_revision"], "工程在准备时变化，未开始导出");
    captured.first.getChildWithName("NATIVEDAW")
        .removeChild(captured.first.getChildWithName("NATIVEDAW").getChildWithName("PERSISTENT_HISTORY"), nullptr);
    const auto text = captured.first.toXmlString();
    require(text.getNumBytesAsUTF8() <= 16 * 1024 * 1024, "Stems快照超过16MiB准备预算");
    batch.reset();
    batch = std::make_unique<Batch>();
    batch->snapshot = captured.first;
    batch->originalEditFile = owner.edit->editFileRetriever ? owner.edit->editFileRetriever() : juce::File{};
    batch->items = items;
    batch->destination = destination;
    batch->stage = parent.getChildFile(".Forma-Stems-" + juce::Uuid().toString());
    require(std::filesystem::create_directory(batch->stage.getFullPathName().toStdString()),
            "独立批次暂存目录创建失败");
    batch->binding = {
        {"batch_id", juce::Uuid().toString().toStdString()},
        {"configuration", a},
        {"processing_chain_hash", juce::SHA256(text.toRawUTF8(), text.getNumBytesAsUTF8()).toHexString().toStdString()},
        {"source_scope", "active clip media and verified freeze caches; opaque plugin-internal assets are not "
                         "independently enumerated"}};
    terminal = nullptr;
    try
    {
        beginNext();
        startTimer(50);
    }
    catch (...)
    {
        batch.reset();
        throw;
    }
    return status();
}
void StemExport::beginNext()
{
    require(batch && !job, "Stems调度状态错误");
    const auto index = batch->files.size();
    if (index == batch->items.size())
    {
        batch->ready = true;
        stopTimer();
        return;
    }
    const auto& item = batch->items[index];
    auto next = std::make_unique<Job>();
    te::Edit::Options options{owner.engine, batch->snapshot.createCopy()};
    options.role = te::Edit::forRendering;
    options.numAudioTracks = 0;
    const auto f = batch->originalEditFile;
    options.editFileRetriever = [f] { return f; };
    next->edit = te::Edit::createEdit(std::move(options));
    require(bool(next->edit), "原生Stems渲染副本创建失败");
    for (auto* p : te::getAllPlugins(*next->edit, true))
    {
        require(!dynamic_cast<te::InsertPlugin*>(p), "Stems离线导出不支持硬件插入");
        if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p))
            require(!ext->isEnabled() || ext->isFrozen() || bool(ext->state.getProperty("ndaw_print_baked", false)) ||
                        ext->getAudioPluginInstance(),
                    "副本外部插件未实际载入");
    }
    next->sourceRefs = sourceFiles(*next->edit);
    for (auto* t : te::getAudioTracks(*next->edit))
        for (auto* clip : t->getClips())
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(clip); wave && next->inputs.size() < 64)
            {
                const auto playback = wave->getPlaybackFile();
                next->inputs.push_back({{"clip", wave->itemID.toString().toStdString()},
                                        {"playback_file", playback.getFile().getFullPathName().toStdString()},
                                        {"exists", playback.getFile().existsAsFile()},
                                        {"valid", playback.isValid()},
                                        {"rate", playback.getInfo().sampleRate},
                                        {"proxy", wave->usesTimeStretchedProxy()}});
            }
    if (!item["track"].is_null())
        captureOutput(*next->edit, item["track"]);

    next->raw = batch->stage.getChildFile(".render-" + juce::String(int(index)) + "-float.wav");
    next->encoded = batch->stage.getChildFile(juce::String(item["filename"].get<std::string>()));
    const auto& config = batch->binding["configuration"];
    next->start = config["start_samples"];
    next->frames = config["end_samples"].get<int64_t>() - next->start + config["tail_samples"].get<int64_t>();
    next->renderStatus = std::make_unique<te::Edit::ScopedRenderStatus>(*next->edit, false);
    te::Renderer::Parameters p(*next->edit);
    p.destFile = next->raw;
    p.audioFormat = &next->wav;
    p.bitDepth = 32;
    p.sampleRateForAudio = 48000;
    p.canRenderInMono = false;
    p.useMasterPlugins = item["track"].is_null();
    p.tracksToDo = te::toBitSet(te::getAllTracks(*next->edit));
    p.time = {tracktion::TimePosition::fromSeconds(next->start / 48000.),
              tracktion::TimePosition::fromSeconds((next->start + next->frames + 96000) / 48000.)};
    next->task = std::make_unique<te::Renderer::RenderTask>("Forma Stems", p, &next->progress, nullptr);
    auto* flag = next.get();
    next->task->setCancellationCheck(
        [flag]
        {
            while (flag->pause && !flag->cancel && !flag->expired())
                juce::Thread::sleep(5);
            return flag->cancel || flag->expired();
        });
    job = std::move(next);
    if (!worker)
        worker = std::make_unique<Worker>();
    worker->submit(job.get());
}
void StemExport::poll()
{
    if (!job)
        return;
    job->pause = owner.edit->getTransport().isPlaying() || owner.audioConfigurationPending();
    const auto& config = batch->binding["configuration"];
    const bool stale = config["session_token"] != owner.sessionToken() || config["base_revision"] != owner.revision;
    if (stale)
        job->cancel = true;
    if (!job->done.load(std::memory_order_acquire))
        return;
    const auto result = job->result;
    const bool cancelled = job->cancel.load();
    job.reset();
    if (stale || cancelled || result["state"] != "verified")
    {
        terminal = {{"state", stale       ? "stale"
                              : cancelled ? "cancelled"
                                          : "failed"},
                    {"error", stale ? "工程已变化，未发布导出" : result.value("error", std::string("任务已取消"))},
                    {"batch_id", batch->binding["batch_id"]}};
        batch.reset();
        stopTimer();
        return;
    }
    if (batch->sourceHashes.is_null())
        batch->sourceHashes = result["sources"];
    if (batch->sourceHashes != result["sources"])
    {
        terminal = {{"state", "failed"}, {"error", "批次中的源哈希不一致"}};
        batch.reset();
        stopTimer();
        return;
    }
    auto file = batch->items[batch->files.size()];
    file["measurement"] = result["measurement"];
    file["preparation_ms"] = result["elapsed_ms"];
    file["measurement"]["file_verification"]["tap_point"] = file["tap_point"];
    file["measurement"]["file_verification"]["publication"] = "pending exclusive atomic batch directory rename";
    file["measurement"]["float_encoding_risk"]["meaning"] = "Actual floating tap frames exceeding full scale before "
                                                            "PCM24 encoding; integer peaks cannot reveal saturation.";
    file["measurement"]["continuation"]["tap_point"] = file["tap_point"];
    batch->files.push_back(file);
    try
    {
        beginNext();
    }
    catch (const std::exception& e)
    {
        terminal = {{"state", "failed"}, {"error", e.what()}};
        batch.reset();
        stopTimer();
    }
}
void StemExport::timerCallback()
{
    poll();
}
void StemExport::prioritizePlayback(bool playing)
{
    owner.checkThread();
    if (job)
        job->pause = playing;
}
Json StemExport::status()
{
    owner.checkThread();
    poll();
    Json receipt = terminal;
    if (batch)
        receipt = {
            {"state", batch->published ? "published"
                      : batch->ready   ? "ready"
                                       : "rendering"},
            {"binding", batch->binding},
            {"files", batch->files},
            {"destination", batch->destination.getFullPathName().toStdString()},
            {"audio_verified", batch->ready},
            {"project_recorded", owner.metadata.getChildWithName("STEM_EXPORT")["batch_id"].toString().toStdString() ==
                                     batch->binding["batch_id"].get<std::string>()}};
    return {{"busy", bool(job)},
            {"state", receipt.is_object() ? receipt.value("state", std::string("idle")) : "idle"},
            {"completed_files", batch ? batch->files.size() : 0},
            {"total_files", batch ? batch->items.size() : 0},
            {"file_progress", job ? job->progress.load() : 0},
            {"render_inputs", job ? job->inputs : Json(nullptr)},
            {"phase", job ? Json(std::array<const char*, 4>{"source_hash", "render", "measure",
                                                            "validate"}[size_t(job->phase.load())])
                          : Json(nullptr)},
            {"paused_for_playback", job && job->pause.load()},
            {"receipt", receipt},
            {"budgets",
             {{"workers", 1},
              {"prepared_graphs", 1},
              {"seconds_per_file", 60},
              {"tracks", 64},
              {"snapshot_bytes", 16 * 1024 * 1024}}}};
}
Json StemExport::cancel()
{
    owner.checkThread();
    poll();
    if (job)
        job->cancel = true;
    else
    {
        batch.reset();
        terminal = {{"state", "cancelled"}, {"error", "Stems面板已关闭；已发布文件保留"}};
    }
    return status();
}
Json StemExport::ready(const std::string& key) const
{
    require(batch && batch->ready && batch->binding["batch_id"] == key, "没有已核验的完整Stems批次");
    const auto& c = batch->binding["configuration"];
    require(c["session_token"] == owner.sessionToken() && c["base_revision"] == owner.revision,
            "工程已变化，未覆盖人工编辑");
    require(hashes(batch->sourceHashes) == batch->sourceHashes, "提交前Stems源媒体已改变");
    const auto dir = batch->published ? batch->destination : batch->stage;
    for (const auto& row : batch->files)
    {
        const auto f = dir.getChildFile(juce::String(row["filename"].get<std::string>()));
        require(checkedMediaHash(f) == row["measurement"]["file_verification"]["sha256"].get<std::string>(),
                "待发布Stems文件校验失败");
    }
    Json record = {{"schema", 1},
                   {"batch_id", key},
                   {"configuration", c},
                   {"directory", batch->destination.getFullPathName().toStdString()},
                   {"processing_chain_hash", batch->binding["processing_chain_hash"]},
                   {"sources", batch->sourceHashes},
                   {"files", batch->files},
                   {"audio_verified", true},
                   {"external_files_undo", "retained"}};
    for (auto& row : record["files"])
    {
        row["path"] = (batch->destination.getChildFile(juce::String(row["filename"].get<std::string>())))
                          .getFullPathName()
                          .toStdString();
        row["measurement"]["file_verification"]["status"] = batch->published ? "verified_published" : "verified_staged";
        row["measurement"]["file_verification"]["publication"] =
            "exclusive atomic batch directory rename; external files retained on Undo";
    }
    require(record.dump().size() <= 4 * 1024 * 1024, "Stems记录超过4MiB预算，未发布");
    return record;
}
Json StemExport::publish(const std::string& key)
{
    owner.checkThread();
    auto record = ready(key);
    for (auto& row : record["files"])
        row["measurement"]["file_verification"]["status"] = "verified_published";
    if (!batch->published)
    {
        const auto manifest = batch->stage.getChildFile("manifest.json");
        const auto text = juce::String(record.dump(2));
        if (manifest.exists())
            require(manifest.loadFileAsString() == text, "批次manifest已改变，未覆盖");
        else
            require(manifest.replaceWithText(text), "批次manifest写盘失败");
        exclusiveRename(batch->stage, batch->destination);
        batch->published = true;
    }
    return record;
}
Json Commands::prepareStemExport(const Json& a)
{
    checkThread();
    if (!stemExport)
        stemExport = std::make_unique<StemExport>(*this);
    return stemExport->prepare(a);
}
Json Commands::stemExportStatus()
{
    checkThread();
    return stemExport ? stemExport->status() : Json{{"busy", false}, {"state", "idle"}, {"receipt", nullptr}};
}
Json Commands::cancelStemExport()
{
    checkThread();
    return stemExport ? stemExport->cancel() : stemExportStatus();
}
} // namespace ndaw::v2
