#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
struct MidiPitchRange
{
    int low = 0, high = 127;
    int count() const
    {
        return high - low + 1;
    }
    Json json(const std::string& mode = "notes") const
    {
        return {{"low", low}, {"high", high}, {"mode", mode}};
    }
};
struct MidiPitchAxis
{
    juce::Rectangle<int> area;
    MidiPitchRange range;
    double pixelAt(int pitch) const
    {
        return area.getY() + (range.high - pitch + .5) * area.getHeight() / range.count();
    }
    int pitchAt(double y) const
    {
        return std::clamp(range.high - int(std::floor((y - area.getY()) * range.count() / area.getHeight())), range.low,
                          range.high);
    }
};
struct MidiZoom
{
    static bool isMidi(const Json& track)
    {
        return track["type"] == "midi" || track["type"] == "instrument";
    }
    static Json entry(const Json& zoom, const std::string& id)
    {
        return zoom["tracks"].value(id, MidiPitchRange{}.json());
    }
    static MidiPitchRange range(const Json& zoom, const std::string& id)
    {
        const auto e = entry(zoom, id);
        return {e["low"], e["high"]};
    }
    static bool notesView(const Json& view, const std::string& id)
    {
        return view["track_views"].value(id, std::string{}).empty() && entry(view["midi_zoom"], id)["mode"] == "notes";
    }
    static MidiPitchRange centered(double center, int count)
    {
        count = std::clamp(count, 4, 128);
        const int low = std::clamp(int(std::lround(center - (count - 1) * .5)), 0, 128 - count);
        return {low, low + count - 1};
    }
    static MidiPitchRange scaled(MidiPitchRange old, double ratio)
    {
        return centered((old.low + old.high) * .5, int(std::lround(std::clamp(old.count() * ratio, 4., 128.))));
    }
    static MidiPitchRange fit(const Json& clips)
    {
        int low = 127, high = 0;
        bool found = false;
        for (const auto& c : clips)
            if (c["kind"] == "midi")
                for (const auto& n : c["notes"])
                {
                    found = true;
                    low = std::min(low, n["pitch"].get<int>());
                    high = std::max(high, n["pitch"].get<int>());
                }
        if (!found)
            return {};
        return centered((low + high) * .5, std::max(12, high - low + 5));
    }
    static Json all(const Json& facts, const Json& view, int command)
    {
        auto zoom = view["midi_zoom"];
        for (const auto& track : facts["tracks"])
            if (isMidi(track) && notesView(view, track["id"]))
            {
                const std::string id = track["id"];
                zoom["tracks"][id] =
                    (command == 259 ? fit(track["clips"]) : scaled(range(zoom, id), command == 257 ? .5 : 2.)).json();
            }
        return zoom;
    }
    static juce::Rectangle<int> area(juce::Rectangle<int> clip)
    {
        return clip.withTrimmedTop(24).withTrimmedBottom(4).reduced(4, 0);
    }
    static juce::Rectangle<float> noteBounds(const Json& note, const Json& clip, MidiPitchAxis axis)
    {
        const double length = clip["length_samples"].get<int64_t>();
        const double fraction =
            (note["position_samples"].get<int64_t>() - clip["start_samples"].get<int64_t>()) / length;
        const double row = double(axis.area.getHeight()) / axis.range.count();
        return {float(axis.area.getX() + fraction * axis.area.getWidth()),
                float(axis.pixelAt(note["pitch"]) - row * .375),
                float(std::max(2., note["length_samples"].get<int64_t>() / length * axis.area.getWidth())),
                float(std::max(1., row * .75))};
    }
    static void draw(juce::Graphics& g, const Json& clip, juce::Rectangle<int> rect, MidiPitchRange range)
    {
        const MidiPitchAxis axis{area(rect), range};
        if (axis.area.isEmpty())
            return;
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(axis.area);
        for (const auto& note : clip["notes"])
            if (note["pitch"] >= range.low && note["pitch"] <= range.high)
                g.fillRect(noteBounds(note, clip, axis));
    }
};
class MidiZoomControls final : public juce::Component
{
public:
    MidiZoomControls()
    {
        setComponentID("timeline.midi.zoom");
        for (auto* b : {&up, &down, &fit})
            addAndMakeVisible(b);
        up.setTooltip(text("MIDI Notes 显示放大 · ⌘⇧] · 不改变音高"));
        down.setTooltip(text("MIDI Notes 显示缩小 · ⌘⇧["));
        fit.setTooltip(text("适配实际全部音符 · ⌃⌘⇧[ · 仅 Notes 视图"));
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        for (auto [button, id] : std::vector<std::pair<juce::TextButton*, int>>{{&up, 257}, {&down, 258}, {&fit, 259}})
        {
            button->setComponentID("ui.command:" + juce::String(id));
            button->setCommandToTrigger(&manager, id, true);
        }
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(accent());
        g.setFont(juce::FontOptions(10));
        g.drawText(text("♪"), 0, 0, getWidth(), 12, juce::Justification::centred);
    }
    void resized() override
    {
        up.setBounds(0, 12, getWidth(), 18);
        down.setBounds(0, 30, getWidth(), 18);
        fit.setBounds(0, 48, getWidth(), 18);
    }

private:
    juce::TextButton up{"+"}, down{text("−")}, fit{"N"};
};
} // namespace ndaw::desktop
