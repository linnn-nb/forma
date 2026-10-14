// SPDX-License-Identifier: AGPL-3.0-only
#include "PreparedAudioSources.h"
#include "WarpState.h"
#include "ElasticState.h"
namespace ndaw::v2
{
void validateWarpEdit(te::Edit&);
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
} // namespace
PreparedAudioSources::PreparedAudioSources(te::Edit& live, te::Edit& snapshot) : engine(live.engine)
{
    require(juce::MessageManager::getInstance()->isThisTheMessageThread(), "来源准备只在message thread提交");
    require(!snapshot.shouldPlay() && &snapshot.engine == &engine, "来源准备需要同一Engine的只读快照");
    validateWarpEdit(live);
    validateWarpEdit(snapshot);
    std::map<std::string, std::string> media;
    for (auto* t : te::getAudioTracks(snapshot))
        for (auto* piece : t->getClips())
            if (auto* c = dynamic_cast<te::WaveAudioClip*>(piece);
                c && (c->getWarpTime() || elastic_state::isWindow(c->state)))
            {
                const auto p =
                    c->getWarpTime() ? c->state.getChildWithName("NDAW_WARP") : elastic_state::descriptor(c->state);
                const auto path = c->getOriginalFile().getFullPathName().toStdString();
                if (!media.contains(path))
                    media[path] = Commands::mediaHash(c->getOriginalFile());
                require(media[path] == p["media_hash"].toString().toStdString(), "来源媒体已变化");
                const te::AudioFile original(engine, c->getOriginalFile());
                require(original.isValid() && original.getNumChannels() >= 1 && original.getNumChannels() <= 8,
                        "来源媒体不可读或超出原生伸缩器已支持的8声道范围");
                Entry e(engine);
                e.clip = c;
                e.sampleRate = original.getSampleRate();
                e.channels = original.getNumChannels();
                if (c->getWarpTime())
                {
                    e.kind = "warp";
                    e.chain = p["render_hash"].toString().toStdString();
                    e.file = te::RenderManager::getAudioFileForHash(engine, live.getTempDirectory(false), c->getHash());
                    e.length = c->getWarpTimeManager().getWarpEndMarkerTime().inSeconds();
                }
                else
                {
                    e.kind = "elastic_proxy";
                    e.chain = p["processing_hash"].toString().toStdString();
                    e.file = te::TemporaryFileManager::getFileForCachedClipRender(
                        live, p["origin_clip"].toString(), elastic_state::integer(p, "proxy_hash"));
                    e.length = p["full_length_seconds"];
                }
                require(c->pinPreparedSourceDestination(e.file), "来源快照不能引用目标cache");
                auto same =
                    std::find_if(entries.begin(), entries.end(), [&](const auto& x) { return x.file == e.file; });
                if (same != entries.end())
                {
                    require(same->chain == e.chain && same->kind == e.kind, "来源cache key对应不同处理链");
                    continue;
                }
                require(entries.size() < 64, "试听来源准备超过64个完整处理源预算");
                e.borrowed = busy(e);
                e.started = e.borrowed;
                e.ready = !e.borrowed && valid(e);
                if (!e.borrowed && !e.ready)
                    engine.getAudioFileManager().checkFileForChanges(e.file);
                entries.push_back(std::move(e));
            }
}
PreparedAudioSources::~PreparedAudioSources()
{
    cancel();
}
bool PreparedAudioSources::busy(const Entry& e) const
{
    return engine.getAudioFileManager().proxyGenerator.isProxyBeingGenerated(e.file) ||
           engine.getRenderManager().isProxyBeingGenerated(e.file);
}
bool PreparedAudioSources::valid(const Entry& e) const
{
    if (!e.file.getFile().existsAsFile())
        return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(
        wav.createReaderFor(new juce::FileInputStream(e.file.getFile()), true));
    return reader && reader->sampleRate == e.sampleRate && int(reader->numChannels) == e.channels &&
           reader->lengthInSamples >= std::ceil(e.length * e.sampleRate) &&
           (e.kind != "warp" || (reader->bitsPerSample == 32 && reader->usesFloatingPointData));
}
void PreparedAudioSources::advance(size_t availableSlots)
{
    if (cancelled)
        return;
    auto active = activeOwnedTasks();
    for (auto& e : entries)
    {
        if (e.ready)
            continue;
        if (e.started)
        {
            if (busy(e))
                continue;
            if (!e.task && !valid(e))
            {
                // A shared predecessor may have been cancelled. Admit one owned
                // attempt after it retires; an owned failure is never retried.
                e.started = false;
                e.borrowed = false;
            }
            else
            {
                if (e.task)
                    require(e.task->state.load() == te::AudioClipBase::PreparedSourceTask::ready,
                            "source_prepare_failed");
                require(valid(e), "source_prepare_failed");
                e.ready = true;
                continue;
            }
        }
        if (active >= availableSlots)
            continue;
        e.task = e.clip->prepareAuditionSource(e.file);
        require(bool(e.task), "source_prepare_admission_failed");
        e.started = true;
        ++active;
    }
}
void PreparedAudioSources::cancel() noexcept
{
    if (cancelled)
        return;
    cancelled = true;
    for (auto& e : entries)
    {
        if (e.task)
            e.task->cancel = true;
        e.clip = nullptr;
    }
}
size_t PreparedAudioSources::activeOwnedTasks() const
{
    return std::count_if(entries.begin(), entries.end(), [&](const auto& e) { return e.task && busy(e); });
}
bool PreparedAudioSources::ready() const
{
    return !cancelled && std::all_of(entries.begin(), entries.end(), [](const auto& e) { return e.ready; });
}
bool PreparedAudioSources::settled() const
{
    return activeOwnedTasks() == 0;
}
Json PreparedAudioSources::status() const
{
    Json sources = Json::array();
    double weight = 0, done = 0;
    size_t readyCount = 0;
    bool measured = entries.empty();
    for (const auto& e : entries)
    {
        const bool running = e.started && busy(e);
        const double progress =
            e.ready   ? 1.
            : e.task  ? double(e.task->progress.load())
            : running ? (e.kind == "warp" ? engine.getRenderManager().getProportionComplete(e.file, 0.f)
                                          : engine.getAudioFileManager().proxyGenerator.getProportionComplete(e.file))
                      : 0.;
        const bool available = e.ready ||
                               (e.task && e.task->state.load() != te::AudioClipBase::PreparedSourceTask::queued) ||
                               (e.borrowed && running);
        measured |= available;
        weight += e.length;
        done += e.length * std::clamp(progress, 0., 1.);
        if (e.ready)
            ++readyCount;
        sources.push_back({{"path", e.file.getFile().getFullPathName().toStdString()},
                           {"kind", e.kind},
                           {"processing_hash", e.chain},
                           {"state", e.ready     ? "ready"
                                     : cancelled ? "cancelled"
                                     : (!e.started || (e.task && e.task->state.load() ==
                                                                     te::AudioClipBase::PreparedSourceTask::queued))
                                         ? "queued"
                                     : running ? "rendering"
                                               : "verifying"},
                           {"progress", std::clamp(progress, 0., 1.)},
                           {"progress_available", available},
                           {"bytes", e.file.getFile().getSize()}});
    }
    return {{"sources", sources},
            {"ready_sources", readyCount},
            {"total_sources", entries.size()},
            {"progress", weight > 0 ? done / weight : 1.},
            {"progress_available", measured},
            {"active_owned_tasks", activeOwnedTasks()},
            {"cleanup_pending", !settled()}};
}
} // namespace ndaw::v2

namespace ndaw::v2
{
PreparedAudioRetirement::~PreparedAudioRetirement()
{
    stopTimer();
    for (auto& p : retired)
        p->cancel();
}
void PreparedAudioRetirement::collect()
{
    std::erase_if(retired, [](const auto& p) { return p->settled(); });
    if (retired.empty())
        stopTimer();
}
void PreparedAudioRetirement::retire(std::shared_ptr<PreparedAudioSources> p)
{
    p->cancel();
    collect();
    if (!p->settled())
    {
        retired.push_back(std::move(p));
        startTimerHz(20);
    }
}
size_t PreparedAudioRetirement::activeTasks()
{
    collect();
    size_t count = 0;
    for (const auto& p : retired)
        count += p->activeOwnedTasks();
    return count;
}
void PreparedAudioRetirement::timerCallback()
{
    collect();
}
} // namespace ndaw::v2
