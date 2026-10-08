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
                if (thumbs.contains(id))
                    continue;
                auto t = std::make_unique<juce::AudioThumbnail>(512, formats, cache);
                t->addChangeListener(this);
                t->setSource(new juce::FileInputSource(juce::File(text(clip["path"].get<std::string>()))));
                thumbs[id] = std::move(t);
            }
        for (auto it = thumbs.begin(); it != thumbs.end();)
            if (!live.contains(it->first))
            {
                it->second->removeChangeListener(this);
                it = thumbs.erase(it);
            }
            else
                ++it;
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
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        redraw();
    }
    std::function<void()> redraw;
    juce::AudioFormatManager formats;
    juce::AudioThumbnailCache cache{64};
    std::map<std::string, std::unique_ptr<juce::AudioThumbnail>> thumbs;
};

} // namespace ndaw::desktop
