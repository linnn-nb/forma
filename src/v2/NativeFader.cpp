// SPDX-License-Identifier: AGPL-3.0-only
#include "NativeFader.h"
#include <mutex>
namespace ndaw::v2
{
static_assert(std::atomic<const void*>::is_always_lock_free);
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
namespace te = tracktion::engine;
namespace
{
class PreparedFader;
struct Curve
{
    struct Point
    {
        double time;
        float value, shape;
    };
    std::vector<Point> points;
    bool beats = false, bypass = false;
    float at(double seconds, const tracktion::tempo::Sequence* tempo) const noexcept
    {
        const double position =
            beats ? tempo->toBeats(tracktion::TimePosition::fromSeconds(seconds)).inBeats() : seconds;
        if (points.size() == 1 || position < points.front().time)
            return points.front().value;
        auto next = std::lower_bound(points.begin() + 1, points.end(), position,
                                     [](const Point& p, double at) { return p.time < at; });
        if (next == points.end())
            return points.back().value;
        const auto& a = *(next - 1);
        const auto& b = *next;
        if (a.time == b.time)
            return b.value;
        if (a.shape == 0)
            return a.value + (b.value - a.value) * float((position - a.time) / (b.time - a.time));
        const auto control = tracktion::core::getBezierPoint(a.time, a.value, b.time, b.value, a.shape);
        if (std::abs(a.shape) <= .5f)
            return float(tracktion::core::getBezierYFromX(position, a.time, a.value, control.first, control.second,
                                                          b.time, b.value));
        const auto ends = tracktion::core::getBezierEnds(a.time, a.value, b.time, b.value, a.shape);
        if (position >= a.time && position <= ends.x1)
            return a.value;
        if (position >= ends.x2 && position <= b.time)
            return b.value;
        return float(tracktion::core::getBezierYFromX(position, ends.x1, ends.y1, control.first, control.second,
                                                      ends.x2, ends.y2));
    }
};
struct Snapshot
{
    Curve volume, pan;
    std::unique_ptr<tracktion::tempo::Sequence> tempo;
    te::PanLaw law;
    bool polarity = false, supported = true;
};
} // namespace
struct NativeFaderManager::Impl : private juce::Timer
{
    Impl()
    {
        startTimer(20);
    }
    ~Impl() override
    {
        stopTimer();
    }
    std::mutex mutex;
    std::vector<PreparedFader*> receivers;
    void timerCallback() override;
};
namespace
{
class PreparedFader final : public te::VolumeAndPanAutomationProcessor, private juce::ValueTree::Listener
{
public:
    PreparedFader(te::VolumeAndPanPlugin& p, std::shared_ptr<NativeFaderManager::Impl> m)
        : plugin(p), manager(std::move(m)), state(p.state), tempoState(p.edit.tempoSequence.getState())
    {
        state.addListener(this);
        tempoState.addListener(this);
        std::lock_guard lock(manager->mutex);
        manager->receivers.push_back(this);
    }
    ~PreparedFader() override
    {
        {
            std::lock_guard lock(manager->mutex);
            std::erase(manager->receivers, this);
        }
        state.removeListener(this);
        tempoState.removeListener(this);
        jassert(readers.load() == 0);
    }
    void capture(bool force = false)
    {
        std::lock_guard lock(captureMutex); // Never entered by process().
        if (force || dirty.exchange(false))
        {
            auto next = std::make_unique<Snapshot>();
            auto load = [&](te::AutomatableParameter& parameter, Curve& curve)
            {
                const auto& native = parameter.getCurve();
                curve.beats = native.timeBase == te::AutomationCurve::TimeBase::beats;
                curve.bypass = bool(native.bypass.get());
                next->supported &= !parameter.hasActiveModifierAssignments();
                for (int i = 0; i < native.getNumPoints(); ++i)
                {
                    const auto point = native.getPoint(i);
                    Curve::Point p{tracktion::toUnderlying(point.time), point.value, point.curve};
                    next->supported &= std::isfinite(p.time) && std::isfinite(p.value) && std::isfinite(p.shape) &&
                                       std::abs(p.shape) <= 1 &&
                                       (curve.points.empty() || p.time >= curve.points.back().time);
                    curve.points.push_back(p);
                }
            };
            load(*plugin.volParam, next->volume);
            load(*plugin.panParam, next->pan);
            if (next->volume.beats || next->pan.beats)
                next->tempo =
                    std::make_unique<tracktion::tempo::Sequence>(plugin.edit.tempoSequence.getInternalSequence());
            next->law = plugin.getPanLaw();
            next->polarity = plugin.polarity;
            next->supported &= !bool(plugin.applyToMidi);
            if (!bool(plugin.ignoreVca))
                if (auto* owner = plugin.getOwnerTrack())
                    for (auto* parent = owner->getParentFolderTrack(); parent; parent = parent->getParentFolderTrack())
                        next->supported &= parent->getVCAPlugin() == nullptr;
            auto old = std::move(owned);
            owned = std::move(next);
            published.exchange(owned.get(), std::memory_order_seq_cst);
            if (old)
                retired.push_back(std::move(old));
        }
        // Seq-cst reader registration precedes its pointer load. Reclamation is
        // exclusively non-real-time and cannot race an unregistered old reader.
        if (readers.load(std::memory_order_seq_cst) == 0)
            retired.clear();
    }
    void prepare(const te::PluginInitialisationInfo& info) override
    {
        capture(true);
        manualVolume.reset(info.sampleRate, .015);
        manualPan.reset(info.sampleRate, .015);
        manualVolume.setCurrentAndTargetValue(plugin.volParam->getCurrentExplicitValue());
        manualPan.setCurrentAndTargetValue(plugin.panParam->getCurrentExplicitValue());
    }
    bool process(const te::PluginRenderContext& context) noexcept override
    {
        struct Reader
        {
            std::atomic<unsigned>& count;
            Reader(std::atomic<unsigned>& c) : count(c)
            {
                count.fetch_add(1, std::memory_order_seq_cst);
            }
            ~Reader()
            {
                count.fetch_sub(1, std::memory_order_seq_cst);
            }
        } reader(readers);
        const auto* data = published.load(std::memory_order_seq_cst);
        if (!data || !data->supported || !plugin.edit.getAutomationRecordManager().isReadingAutomation() ||
            plugin.volParam->isCurrentlyRecording() || plugin.panParam->isCurrentlyRecording())
            return false;
        const bool volumeAuto = !data->volume.bypass && !data->volume.points.empty(),
                   panAuto = !data->pan.bypass && !data->pan.points.empty();
        if (!volumeAuto && !panAuto)
            return false;
        auto* buffer = context.destBuffer;
        const int count = context.bufferNumSamples;
        if (!buffer || buffer->getNumChannels() == 0 || count <= 0)
            return true;
        const float baseVolume = plugin.volParam->getCurrentExplicitValue(),
                    basePan = plugin.panParam->getCurrentExplicitValue();
        manualVolume.setTargetValue(baseVolume);
        manualPan.setTargetValue(basePan);
        const double first = context.editTime.getStart().inSeconds(),
                     step = context.editTime.getLength().inSeconds() / count;
        float volume = baseVolume, pan = basePan, left = 0, right = 0, gain = 0;
        for (int i = 0; i < count; ++i)
        {
            const double time = first + i * step;
            volume = volumeAuto ? juce::jlimit(0.f, 1.f, data->volume.at(time, data->tempo.get()))
                                : manualVolume.getNextValue();
            pan = panAuto ? juce::jlimit(-1.f, 1.f, data->pan.at(time, data->tempo.get())) : manualPan.getNextValue();
            if (volumeAuto)
                manualVolume.setCurrentAndTargetValue(volume);
            if (panAuto)
                manualPan.setCurrentAndTargetValue(pan);
            te::getGainsFromVolumeFaderPositionAndPan(volume, pan, data->law, left, right);
            gain = te::volumeFaderPositionToGain(volume);
            if (data->polarity)
            {
                left = -left;
                right = -right;
                gain = -gain;
            }
            const int sample = context.bufferStartSample + i;
            buffer->getWritePointer(0)[sample] *= left;
            if (buffer->getNumChannels() > 1)
                buffer->getWritePointer(1)[sample] *= right;
            for (int ch = 2; ch < buffer->getNumChannels(); ++ch)
                buffer->getWritePointer(ch)[sample] *= gain;
        }
        plugin.finishPreparedAutomationBlock(volume, pan, left, right, gain);
        return true;
    }

private:
    te::VolumeAndPanPlugin& plugin;
    std::shared_ptr<NativeFaderManager::Impl> manager;
    juce::ValueTree state, tempoState;
    std::mutex captureMutex;
    std::atomic<bool> dirty{true};
    std::atomic<unsigned> readers{0};
    std::atomic<const Snapshot*> published{nullptr};
    std::unique_ptr<Snapshot> owned;
    std::vector<std::unique_ptr<Snapshot>> retired;
    juce::SmoothedValue<float> manualVolume, manualPan;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override
    {
        dirty.store(true);
    }
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override
    {
        dirty.store(true);
    }
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override
    {
        dirty.store(true);
    }
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override
    {
        dirty.store(true);
    }
    void valueTreeRedirected(juce::ValueTree&) override
    {
        dirty.store(true);
    }
};
} // namespace
void NativeFaderManager::Impl::timerCallback()
{
    std::lock_guard lock(mutex);
    for (auto* receiver : receivers)
        receiver->capture();
}
NativeFaderManager::NativeFaderManager() : impl(std::make_shared<Impl>()) {}
NativeFaderManager::~NativeFaderManager() = default;
std::unique_ptr<te::VolumeAndPanAutomationProcessor> NativeFaderManager::create(te::VolumeAndPanPlugin& p)
{
    return std::make_unique<PreparedFader>(p, impl);
}
} // namespace ndaw::v2
