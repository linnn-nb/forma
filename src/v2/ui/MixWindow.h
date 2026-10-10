#pragma once
#include "Theme.h"
#include "TrackHeader.h"
namespace ndaw::desktop
{
class MixWindow final : public juce::Component
{
public:
    MixWindow(Writer write, std::function<void(std::string)> select)
        : write(std::move(write)), select(std::move(select))
    {
        setComponentID("mix.channels");
        reset.setComponentID("mix.output.reset");
        reset.setButtonText(text("复位峰值 / OVER"));
        reset.setTooltip(text("清除输出峰值保持与超过 0 dBFS 的标记。下一音频回调确认；持续过载会重新亮起。"));
        reset.onClick = [this] { this->write("audio.meters.reset", Json::object()); };
        addAndMakeVisible(reset);
    }
    std::function<void(std::string, juce::Component&, bool)> onTrackOptions;
    std::function<void(std::string, int)> onRecordingCommand;
    std::function<void(std::string, juce::Component&)> onMonitorMenu;
    std::function<void(std::string, int, juce::Component&)> onInsert;
    std::function<void(std::string)> onRouting, onComments;
    void update(const Json& facts, const std::string& selected, const Json& device)
    {
        std::vector<std::string> ids;
        for (const auto& t : facts["tracks"])
            ids.push_back(t["id"]);
        if (ids != trackIDs)
        {
            trackIDs = ids;
            controls.clear();
            for (auto& id : ids)
            {
                auto c = std::make_unique<TrackHeader>(
                    id, true, write, select,
                    [this](std::string id, int index, juce::Component& anchor)
                    {
                        if (onInsert)
                            onInsert(id, index, anchor);
                    },
                    [this](std::string id)
                    {
                        if (onRouting)
                            onRouting(id);
                    },
                    [this](std::string id)
                    {
                        if (onComments)
                            onComments(id);
                    });
                c->onRecordingCommand = [this](auto id, int command)
                {
                    if (onRecordingCommand)
                        onRecordingCommand(id, command);
                };
                c->onMonitorMenu = [this](auto id, auto& component)
                {
                    if (onMonitorMenu)
                        onMonitorMenu(id, component);
                };
                c->onOptions = [this](auto id, auto& component, bool strip)
                {
                    if (onTrackOptions)
                        onTrackOptions(id, component, strip);
                };
                addAndMakeVisible(*c);
                controls.push_back(std::move(c));
            }
            resized();
        }
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto item = facts["tracks"][i];
            item["playing"] = facts["playing"];
            item["recording_controls_pending"] = TrackRecordingState::blocked(facts);
            item["automation_writing"] = !facts["automation_capture"].is_null();
            item["recording"] = !facts["recording_capture"].is_null();
            controls[i]->update(item, trackIDs[i] == selected);
        }
        meters = device.value("output_meters", Json::object());
        physical = device.value("active_output_channels", Json::array());
        auto frames = meters.value("frames", uint64_t(0));
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (frames != lastFrames || meters.value("generation", uint64_t(0)) != lastGeneration)
        {
            lastFrames = frames;
            lastGeneration = meters.value("generation", uint64_t(0));
            lastAdvance = now;
        }
        const auto maxAge = std::max(250., 3000. * device.value("buffer_frames", 0) /
                                               std::max(1., device.value("sample_rate", 48000.)));
        available = device.value("available", false) && device.value("driver_running", false) &&
                    meters.value("available", false) && now - lastAdvance <= maxAge;
        reset.setEnabled(available && !meters.value("reset_pending", false));
        repaint();
    }
    void resized() override
    {
        for (size_t i = 0; i < controls.size(); ++i)
            controls[i]->setBounds(int(i) * 150 + 8, 8, 140, getHeight() - 16);
        reset.setBounds(masterX() + 12, 93, 126, 24);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
        const int x = masterX();
        g.setColour(juce::Colour(0xff23333c));
        g.fillRect(x, 8, 150, getHeight() - 16);
        g.setColour(accent());
        g.fillRect(x, 8, 150, 4);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13, juce::Font::bold));
        g.drawText("OUTPUT", x + 12, 22, 126, 30, juce::Justification::centred);
        g.setFont(juce::FontOptions(11));
        g.setColour(juce::Colour(0xffb8cbd7));
        g.drawText(text("限幅前 · Sample Peak"), x + 8, 61, 134, 24, juce::Justification::centred);
        const int top = 148, bottom = std::max(top + 50, getHeight() - 108), height = bottom - top;
        g.setFont(juce::FontOptions(10));
        for (int d = 0; d >= -60; d -= 12)
        {
            g.setColour(juce::Colour(0xff8394a7));
            g.drawText(juce::String(d), x + 9, top + int(-d / 60. * height) - 8, 33, 16, juce::Justification::right);
        }
        const auto channels = meters.value("channels", Json::array());
        const int count = std::min(2, int(physical.size()));
        for (int i = 0; i < 2; ++i)
        {
            const int bx = x + 57 + i * 35;
            g.setColour(juce::Colour(0xff111921));
            g.fillRect(bx, top, 19, height);
            const int index = i < count ? physical[i].get<int>() : -1;
            const bool present = available && index >= 0 && index < int(channels.size());
            const auto c = present ? channels[index] : Json::object();
            const double display = c.value("display_peak", 0.), hold = c.value("hold", 0.);
            g.setColour(c.value("over", false) ? juce::Colour(0xffe0756b) : accent());
            int h = levelHeight(display, height);
            g.fillRect(bx, bottom - h, 19, h);
            if (present && hold > 0)
            {
                g.setColour(juce::Colour(0xffedca72));
                g.fillRect(bx, bottom - levelHeight(hold, height), 19, 2);
            }
            g.setColour(juce::Colour(0xffd0dce5));
            auto label = index < 0                         ? juce::String("—")
                         : physical == Json::array({0, 1}) ? juce::String(i == 0 ? "L" : "R")
                                                           : juce::String(index + 1);
            g.drawText(label, bx - 3, 122, 25, 22, juce::Justification::centred);
            g.drawText(present ? dbText(hold) : juce::String("—"), bx - 17, bottom + 11, 53, 20,
                       juce::Justification::centred);
            if (c.value("over", false))
            {
                g.setColour(juce::Colour(0xffe0756b));
                g.drawText("OVER", bx - 13, bottom + 32, 45, 20, juce::Justification::centred);
            }
        }
        g.setColour(juce::Colour(0xffb8cbd7));
        g.drawText(text("保持 · dBFS"), x + 8, bottom + 53, 134, 19, juce::Justification::centred);
        auto state = !available                             ? text("等待真实输出回调")
                     : meters.value("reset_pending", false) ? text("复位等待回调确认")
                     : physical.size() > 2                  ? text("其余声道可通过查询查看")
                                                            : text("设备输出 / 非 True Peak");
        g.setFont(juce::FontOptions(10));
        g.drawText(state, x + 5, getHeight() - 31, 140, 18, juce::Justification::centred);
    }

private:
    int masterX() const
    {
        return int(controls.size()) * 150 + 16;
    }
    static int levelHeight(double level, int height)
    {
        return int(std::clamp(((level > 0 ? 20 * std::log10(level) : -100) + 60) / 60., 0., 1.) * height);
    }
    static juce::String dbText(double level)
    {
        return level > 0 ? juce::String(20 * std::log10(level), 1) : text("−∞");
    }
    Writer write;
    std::function<void(std::string)> select;
    std::vector<std::string> trackIDs;
    std::vector<std::unique_ptr<TrackHeader>> controls;
    juce::TextButton reset;
    Json meters = Json::object(), physical = Json::array();
    bool available = false;
    uint64_t lastFrames = 0, lastGeneration = 0;
    double lastAdvance = 0;
};

} // namespace ndaw::desktop
