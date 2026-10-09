#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class Waveforms final : private juce::ChangeListener
{
public:
    explicit Waveforms(std::function<void()> redraw) : redraw(std::move(redraw))
    {
        formats.registerBasicFormats();
    }
    ~Waveforms() override
    {
        for (auto& [id, t] : thumbs)
            t->removeChangeListener(this);
    }
    void update(const Json& facts)
    {
        std::set<std::string> live;
        for (const auto& track : facts["tracks"])
            for (const auto& clip : track["clips"])
            {
                if (clip.contains("path"))
                    live.insert(clip["id"]);
                if (!clip.contains("path"))
                    continue;
                auto id = clip["id"].get<std::string>();
                const juce::File file(text(clip["path"].get<std::string>()));
                if (thumbs.contains(id) && stamps.at(id).path == file.getFullPathName())
                    continue;
                if (thumbs.contains(id))
                    thumbs.at(id)->removeChangeListener(this);
                auto t = std::make_unique<juce::AudioThumbnail>(512, formats, cache);
                t->addChangeListener(this);
                t->setSource(new juce::FileInputSource(juce::File(text(clip["path"].get<std::string>()))));
                thumbs[id] = std::move(t);
                stamps[id] = stamp(file);
            }
        for (auto it = thumbs.begin(); it != thumbs.end();)
            if (!live.contains(it->first))
            {
                it->second->removeChangeListener(this);
                stamps.erase(it->first);
                it = thumbs.erase(it);
            }
            else
                ++it;
    }
    // Display fitting uses the loaded source thumbnail, not mixer output or an analysis artifact.
    // File I/O here is metadata-only, on the message thread, never the audio callback.
    double selectionScale(const Json& track, int64_t first, int64_t last)
    {
        double peak = 0;
        for (const auto& clip : track["clips"])
        {
            if (clip["kind"] != "audio")
                continue;
            const auto begin = std::max(first, clip["start_samples"].get<int64_t>());
            const auto end =
                std::min(last, clip["start_samples"].get<int64_t>() + clip["length_samples"].get<int64_t>());
            if (begin >= end)
                continue;
            const std::string id = clip["id"];
            const juce::File file(text(clip["path"].get<std::string>()));
            if (!clip.value("source_mapping_available", false) || !file.existsAsFile() || !thumbs.contains(id))
                throw std::runtime_error("选区波形不可用或时间映射不支持，缩放未执行");
            if (!(stamp(file) == stamps.at(id)))
            {
                thumbs.at(id)->setSource(new juce::FileInputSource(file));
                stamps[id] = stamp(file);
                throw std::runtime_error("源媒体已变化，波形重新载入后再试");
            }
            auto& thumbnail = *thumbs.at(id);
            if (!thumbnail.isFullyLoaded() || thumbnail.getNumChannels() < 1)
                throw std::runtime_error("正在载入真实波形，请稍后重试缩放");
            const double offset = clip.value("source_offset_seconds", 0.);
            const double speed = clip.value("speed_ratio", 1.);
            const double start = offset + (begin - clip["start_samples"].get<int64_t>()) / 48000. * speed;
            const double stop = offset + (end - clip["start_samples"].get<int64_t>()) / 48000. * speed;
            for (int channel = 0; channel < thumbnail.getNumChannels(); ++channel)
            {
                float low = 0, high = 0;
                thumbnail.getApproximateMinMax(start, stop, channel, low, high);
                peak = std::max(peak, std::max(std::abs(double(low)), std::abs(double(high))) *
                                          std::pow(10., clip.value("gain_db", 0.) / 20.));
            }
        }
        return peak > 0 ? std::clamp(.9 / peak, .03125, 64.) : 1.;
    }
    // The same integer channel partitions as AudioThumbnail::drawChannels.
    // Uses the loaded thumbnail metadata; this gesture never opens a media file.
    juce::Rectangle<int> channelAt(const Json& clip, juce::Rectangle<int> area, int y) const
    {
        const auto it = thumbs.find(clip["id"].get<std::string>());
        if (it == thumbs.end() || area.isEmpty())
            return {};
        const int channels = it->second->getNumChannels();
        for (int i = 0; i < channels; ++i)
        {
            const int top = area.getY() + i * area.getHeight() / channels;
            const int bottom = area.getY() + (i + 1) * area.getHeight() / channels;
            if (y >= top && y < bottom)
                return {area.getX(), top, area.getWidth(), bottom - top};
        }
        return {};
    }
    void draw(juce::Graphics& g, const Json& clip, juce::Rectangle<int> area, double seconds, double elapsed = 0,
              double displayScale = 1.)
    {
        auto id = clip["id"].get<std::string>();
        if (thumbs.contains(id))
        {
            double offset = clip.value("source_offset_seconds", 0.), speed = clip.value("speed_ratio", 1.),
                   gain = std::pow(10., clip.value("gain_db", 0.) / 20);
            thumbs.at(id)->drawChannels(g, area, offset + elapsed * speed, offset + (elapsed + seconds) * speed,
                                        float(gain * displayScale));
        }
    }

private:
    struct Stamp
    {
        juce::String path;
        int64_t modified = 0, bytes = 0;
        bool operator==(const Stamp&) const = default;
    };
    static Stamp stamp(const juce::File& file)
    {
        return {file.getFullPathName(), file.getLastModificationTime().toMilliseconds(), file.getSize()};
    }
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        redraw();
    }
    std::function<void()> redraw;
    juce::AudioFormatManager formats;
    juce::AudioThumbnailCache cache{64};
    std::map<std::string, std::unique_ptr<juce::AudioThumbnail>> thumbs;
    std::map<std::string, Stamp> stamps;
};

} // namespace ndaw::desktop
