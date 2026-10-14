// SPDX-License-Identifier: AGPL-3.0-only
#include "TrackPrint.h"
#include "MasterAnalysis.h"
#include "NativePluginStates.h"
#include "CheckedMedia.h"
#include "SidechainGraph.h"
#include <filesystem>
#include <juce_cryptography/juce_cryptography.h>
#if JUCE_MAC
#include <pthread.h>
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
Json sources(te::Edit& edit)
{
    std::map<std::string, juce::File> files;
    for (auto* t : te::getAudioTracks(edit))
    {
        if (t->isFrozen(te::Track::individualFreeze))
        {
            auto f = te::TemporaryFileManager::getFreezeFileForTrack(*t);
            files[f.getFullPathName().toStdString()] = f;
        }
        else
            for (auto* c : t->getClips())
                if (auto* wave = dynamic_cast<te::WaveAudioClip*>(c))
                    files[wave->getOriginalFile().getFullPathName().toStdString()] = wave->getOriginalFile();
    }
    Json result = Json::array();
    for (const auto& [path, f] : files)
    {
        require(f.existsAsFile(), "印刻依赖的源文件缺失");
        result.push_back(
            {{"path", path}, {"bytes", f.getSize()}, {"modified_ms", f.getLastModificationTime().toMilliseconds()}});
    }
    return result;
}
Json hashSources(Json sources, const std::function<void()>& checkpoint = {})
{
    for (auto& item : sources)
    {
        juce::File f(juce::String(item["path"].get<std::string>()));
        require(f.existsAsFile() && f.getSize() == item["bytes"].get<int64_t>() &&
                    f.getLastModificationTime().toMilliseconds() == item["modified_ms"].get<int64_t>(),
                "印刻期间源文件发生变化");
        item["sha256"] = checkedMediaHash(f, checkpoint);
    }
    return sources;
}
// Copy-only Edit; preserve the entire upstream and sidechain graph, capture at pre-fader.
void isolate(te::Edit& edit, const std::string& targetID)
{
    te::AudioTrack* target = nullptr;
    std::set<int> used;
    for (auto* t : te::getAllTracks(edit))
    {
        t->setSolo(false);
        t->setSoloIsolate(true);
    }
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
            require(!dynamic_cast<te::InsertPlugin*>(p), "离线印刻不支持硬件插入");
            if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p))
                require(!ext->isEnabled() || ext->isFrozen() || ext->getAudioPluginInstance(),
                        "印刻副本的外部插件未载入");
        }
    }
    require(target, "印刻目标轨道失效");
    target->setMute(false);
    int bus = 0;
    while (used.contains(bus))
        ++bus;
    const int boundary = target->pluginList.indexOf(target->getVolumePlugin());
    require(boundary >= 0, "印刻缺少原生推子边界");
    auto capture = edit.insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(edit), nullptr, false);
    require(capture, "印刻捕获轨创建失败");
    auto returned = edit.getPluginCache().createNewPlugin(te::AuxReturnPlugin::xmlTypeName, {});
    auto sent = edit.getPluginCache().createNewPlugin(te::AuxSendPlugin::xmlTypeName, {});
    require(returned && sent, "印刻原生捕获总线创建失败");
    dynamic_cast<te::AuxReturnPlugin&>(*returned).busNumber = bus;
    auto& send = dynamic_cast<te::AuxSendPlugin&>(*sent);
    send.busNumber = bus;
    send.gain->setParameter(te::decibelsToVolumeFaderPosition(0), juce::dontSendNotification);
    capture->pluginList.insertPlugin(returned, 0, nullptr);
    target->pluginList.insertPlugin(sent, boundary, nullptr);
    for (auto* t : te::getAllTracks(edit))
        if (t != capture.get())
            if (auto* out = te::getTrackOutput(*t); out && !out->getDestinationTrack())
                out->setOutputToNone();
    capture->getOutput().setOutputToDefaultDevice(false);
}
} // namespace
struct TrackPrint::Job
{
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<te::Edit::ScopedRenderStatus> renderStatus;
    std::unique_ptr<te::Renderer::RenderTask> task;
    juce::WavAudioFormat wav;
    std::thread worker;
    std::optional<te::ScopedThreadExitStatusEnabler> exitEnabler;
    std::atomic<bool> cancel{false}, pause{false}, done{false};
    std::atomic<float> progress{0};
    std::atomic<int> stageIndex{0};
    Json binding, result;
    juce::File directory, stage, file;
    double began = juce::Time::getMillisecondCounterHiRes();
    bool retained = false;
    bool expired() const
    {
        return juce::Time::getMillisecondCounterHiRes() - began > 60000;
    }
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
        edit.reset();
        if (!retained && directory != juce::File{})
            directory.deleteRecursively();
    }
    void launch()
    {
        auto gate = std::make_shared<juce::WaitableEvent>();
        worker = std::thread(
            [this, gate]
            {
                gate->wait();
#if JUCE_MAC
                pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
#endif
                try
                {
                    const auto checkpoint = [this]
                    {
                        while (pause && !cancel && !expired())
                            juce::Thread::sleep(5);
                        require(!cancel && !expired(), "印刻取消或超过60秒预算");
                    };
                    const auto first = hashSources(binding["sources"], checkpoint);
                    stageIndex = 1;
                    while (task->runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
                        require(!cancel && !expired(), "印刻取消或超过60秒预算");
                    require(!cancel && !expired() && stage.existsAsFile(), "印刻未完成，未产生可提交文件");
                    require(task->errorMessage.isEmpty(), task->errorMessage.toRawUTF8());
                    stageIndex = 2;
                    const auto measured = Commands::analyse(stage, checkpoint);
                    require(measured["frames"] == binding["frames"] && measured["sample_rate"] == 48000 &&
                                measured["channels"] == 2,
                            "印刻文件格式或时长校验失败");
                    require(first == hashSources(binding["sources"], checkpoint), "印刻期间源媒体哈希改变");
                    stageIndex = 3;
                    std::filesystem::create_hard_link(stage.getFullPathName().toStdString(),
                                                      file.getFullPathName().toStdString());
                    stage.deleteFile();
                    result = binding;
                    result["state"] = "ready";
                    result["sources"] = first;
                    result["path"] = file.getFullPathName().toStdString();
                    result["media_hash"] = checkedMediaHash(file, checkpoint);
                    result["measurement"] = measured;
                    result["elapsed_ms"] = juce::Time::getMillisecondCounterHiRes() - began;
                    result["audio_verified"] = true;
                }
                catch (const std::exception& e)
                {
                    result = {{"state", cancel.load() ? "cancelled" : "failed"},
                              {"error", e.what()},
                              {"render_id", binding["render_id"]}};
                }
                done.store(true, std::memory_order_release);
            });
        exitEnabler.emplace(worker.get_id());
        gate->signal();
    }
};
TrackPrint::TrackPrint(Commands& c) : owner(c) {}
TrackPrint::~TrackPrint()
{
    stopTimer();
    job.reset();
}
void TrackPrint::poll()
{
    if (!job)
        return;
    job->pause = owner.edit->getTransport().isPlaying();
    if (job->done.load(std::memory_order_acquire))
    {
        receipt = job->result;
        if (job->cancel.load() && receipt["state"] == "ready")
            receipt = {{"state", "cancelled"}, {"error", "印刻已取消，未修改工程"}};
        job->retained = receipt["state"] == "ready";
        job.reset();
        stopTimer();
    }
}
void TrackPrint::timerCallback()
{
    poll();
}
Json TrackPrint::status()
{
    owner.checkThread();
    poll();
    return {{"busy", bool(job)},
            {"state", job                 ? "rendering"
                      : receipt.is_null() ? "idle"
                                          : receipt["state"].get<std::string>()},
            {"progress", job                 ? job->progress.load()
                         : receipt.is_null() ? 0
                                             : 1},
            {"phase", job ? Json(std::array<const char*, 4>{"source_hash", "render", "measure",
                                                            "publish"}[size_t(job->stageIndex.load())])
                          : Json(nullptr)},
            {"paused_for_playback", job && job->pause.load()},
            {"receipt", receipt},
            {"deadline_seconds", 60}};
}
Json TrackPrint::cancel()
{
    owner.checkThread();
    poll();
    if (job)
        job->cancel = true;
    else
        receipt = nullptr;
    return status();
}
Json TrackPrint::ready(const std::string& key) const
{
    require(receipt.is_object() && receipt.value("state", std::string{}) == "ready" && receipt["render_id"] == key,
            "尚未获得真实可提交的印刻文件");
    require(receipt["session_token"] == owner.sessionToken() && receipt["base_revision"] == owner.revision,
            "工程已变化，需重新印刻；未覆盖人工操作");
    juce::File f(juce::String(receipt["path"].get<std::string>()));
    require(Commands::mediaHash(f) == receipt["media_hash"].get<std::string>(), "印刻文件改变或缺失");
    require(receipt["sources"] == hashSources(receipt["sources"]), "提交前源媒体改变");
    return receipt;
}
Json TrackPrint::prepare(const Json& args)
{
    owner.checkThread();
    poll();
    require(!job, "已有印刻任务；请等待或取消");
    require(args.size() == 6 && args.contains("track") && args.contains("action") && args.contains("end_samples") &&
                args.contains("directory") && args.contains("session_token") && args.contains("base_revision"),
            "印刻参数不完整");
    require(args["session_token"] == owner.sessionToken() && args["base_revision"] == owner.revision,
            "印刻界面版本已失效");
    require(args["action"] == "freeze" || args["action"] == "bounce", "未知印刻用途");
    require(!owner.edit->getTransport().isPlaying() && owner.recordingCapture.is_null() && owner.capture.is_null() &&
                owner.parameterCapture.is_null(),
            "请先停止并完成参数手势");
    require(!owner.masterAnalysis || !owner.masterAnalysis->status()["busy"].get<bool>(), "已有分析/导出任务");
    require(!owner.playbackPreparation()["busy"].get<bool>(), "先等待弹力音频缓存准备");
    owner.validateSidechainRuntime();
    owner.validateFreezeRuntime();
    owner.validateExternalRuntime();
    auto* target = owner.track(args["track"]);
    require(target, "所选轨道不存在");
    require(args["action"] != "freeze" || !target->isFrozen(te::Track::anyFreeze), "轨道已冻结");
    require(!bool(target->state.getProperty("ndaw_locked", false)), "轨道已锁定");
    require(!bool(target->state.getProperty("ndaw_armed", false)) &&
                target->state.getProperty("ndaw_monitor", "off").toString() == "off",
            "先解除录音待命并关闭输入监听");
    const int64_t end = args["end_samples"];
    require(end > 0, "印刻结束必须大于零");
    // Native getInputTracks covers direct outputs; include actual Aux-send dependencies too.
    const auto edges = sidechain_graph::nativeEdges(*owner.edit, true);
    std::set<std::string> upstream{id(*target)};
    bool added = true;
    while (added)
    {
        added = false;
        for (const auto& [source, destinations] : edges)
            if (!upstream.contains(source))
                for (const auto& dest : destinations)
                    if (upstream.contains(dest))
                    {
                        upstream.insert(source);
                        added = true;
                        break;
                    }
    }
    int64_t nativeEnd = 0;
    for (const auto& key : upstream)
        if (auto* t = owner.track(key))
            nativeEnd = std::max(nativeEnd,
                                 t->isFrozen(te::Track::individualFreeze)
                                     ? int64_t(t->state.getProperty("ndaw_freeze_frames", juce::int64(0)))
                                     : int64_t(std::llround(t->getLengthIncludingInputTracks().inSeconds() * 48000.)));
    require(nativeEnd > 0 && end >= nativeEnd, "印刻必须覆盖整个目标轨道；尾音可延长");
    require(!target->state.getChildWithName("NDAW_BOUNCE").isValid(), "先还原当前原地Bounce再制作下一份");
    // Source playlists may include crossfades. Linked multi-track Comp requires a grouped print transaction.
    const int faderIndex = target->pluginList.indexOf(target->getVolumePlugin());
    int printedBoundary = faderIndex;
    while (printedBoundary > 0 && (dynamic_cast<te::AuxSendPlugin*>(target->pluginList[printedBoundary - 1]) ||
                                   dynamic_cast<te::LevelMeterPlugin*>(target->pluginList[printedBoundary - 1])))
        --printedBoundary;
    for (int n = 0; n < printedBoundary; ++n)
        require(!dynamic_cast<te::AuxSendPlugin*>(target->pluginList[n]),
                "效果器中间的发送tap未资格，请先移至推子前边界");
    const auto lists = target->state.getChildWithName("NDAW_PLAYLISTS");
    for (auto list : lists)
        require(list.getProperty("linked_set").toString().isEmpty(), "先解除关联Comp再单轨印刻");
    juce::File parent(juce::String(args["directory"].get<std::string>()));
    require(parent.isDirectory(), "印刻目录不存在");
    const auto renderID = juce::Uuid().toString().toStdString();
    auto work = std::make_unique<Job>();
    work->directory = parent.getChildFile("Forma-Print-" + juce::String(renderID));
    require(std::filesystem::create_directory(work->directory.getFullPathName().toStdString()), "无法创建独立印刻目录");
    work->stage = work->directory.getChildFile(".rendering.wav");
    work->file = work->directory.getChildFile("printed.wav");
    auto captured = owner.recoverySnapshot();
    require(args["base_revision"] == owner.revision, "原生状态准备期间工程已变化，请重试印刻");
    auto meta = captured.first.getChildWithName("NATIVEDAW");
    meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
    te::Edit::Options options{owner.engine, captured.first};
    options.role = te::Edit::forRendering;
    options.numAudioTracks = 0;
    const auto editFile = (owner.edit->editFileRetriever ? owner.edit->editFileRetriever() : juce::File{});
    options.editFileRetriever = [editFile] { return editFile; };
    work->edit = te::Edit::createEdit(std::move(options));
    require(bool(work->edit), "印刻工程副本创建失败");
    work->binding = {{"render_id", renderID},           {"action", args["action"]},
                     {"track", args["track"]},          {"session_token", args["session_token"]},
                     {"base_revision", owner.revision}, {"frames", end},
                     {"tap_point", "track_pre_fader"},  {"sources", sources(*work->edit)},
                     {"processing_chain_hash", ""}};
    // Bind the printed audio to the actual source graph, independently of its media hashes.
    const auto chain = captured.first.toXmlString().toStdString();
    work->binding["processing_chain_hash"] = juce::SHA256(chain.data(), chain.size()).toHexString().toStdString();
    isolate(*work->edit, args["track"]);
    work->renderStatus = std::make_unique<te::Edit::ScopedRenderStatus>(*work->edit, false);
    te::Renderer::Parameters p(*work->edit);
    p.destFile = work->stage;
    p.audioFormat = &work->wav;
    p.bitDepth = 32;
    p.sampleRateForAudio = 48000;
    p.canRenderInMono = false;
    p.useMasterPlugins = false;
    p.tracksToDo = te::toBitSet(te::getAllTracks(*work->edit));
    p.time = {{}, tracktion::TimePosition::fromSeconds(end / 48000.)};
    work->task = std::make_unique<te::Renderer::RenderTask>("Forma track print", p, &work->progress, nullptr);
    auto* flag = work.get();
    work->task->setCancellationCheck(
        [flag]
        {
            while (flag->pause && !flag->cancel && !flag->expired())
                juce::Thread::sleep(5);
            return flag->cancel || flag->expired();
        });
    receipt = nullptr;
    job = std::move(work);
    job->launch();
    startTimer(50);
    return status();
}
Json Commands::prepareTrackPrint(const Json& args)
{
    checkThread();
    if (!trackPrint)
        trackPrint = std::make_unique<TrackPrint>(*this);
    return trackPrint->prepare(args);
}
Json Commands::trackPrintStatus()
{
    checkThread();
    return trackPrint ? trackPrint->status() : Json{{"busy", false}, {"state", "idle"}, {"receipt", nullptr}};
}
Json Commands::cancelTrackPrint()
{
    checkThread();
    return trackPrint ? trackPrint->cancel() : trackPrintStatus();
}
} // namespace ndaw::v2
