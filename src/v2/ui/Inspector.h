#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class InspectorParameters final : public juce::Component
{
public:
    explicit InspectorParameters(Writer writer) : write(std::move(writer))
    {
        for (auto* c : std::initializer_list<juce::Component*>{&previous, &next, &pageLabel})
            addAndMakeVisible(c);
        previous.setComponentID("plugin.parameters.previous");
        next.setComponentID("plugin.parameters.next");
        pageLabel.setComponentID("plugin.parameters.page");
        previous.onClick = [this]
        {
            if (page > 0)
            {
                --page;
                refreshPage();
            }
        };
        next.onClick = [this]
        {
            ++page;
            refreshPage();
        };
    }
    void update(const Json& plugin, bool playing, int width, const std::string& track = {}, bool writing = false,
                const std::string& mode = "read", bool nativeActive = false)
    {
        lastPlugin = plugin;
        lastPlaying = playing;
        lastWidth = width;
        lastTrack = track;
        lastWriting = writing;
        lastMode = mode;
        lastNativeActive = nativeActive;
        stopped = !playing;
        trackID = track;
        liveWrite = playing && writing && mode != "read";
        std::string signature = plugin.is_null() ? "" : plugin["id"].get<std::string>();
        if (signature != pluginID)
        {
            page = 0;
            pluginID = signature;
        }
        int count = plugin.is_null() ? 0 : int(plugin["parameters"].size());
        int pages = std::max(1, (count + 63) / 64);
        page = std::clamp(page, 0, pages - 1);
        paged = count > 64;
        auto signatureWithPage = signature + ":" + std::to_string(page);
        if (signatureWithPage != rowSignature)
        {
            rowSignature = signatureWithPage;
            rows.clear();
            if (!plugin.is_null())
            {
                if (plugin.contains("delay_time_ms"))
                    addRow({{"id", "@delay_time"},
                            {"name", "Time · ms"},
                            {"minimum", 1},
                            {"maximum", 2000},
                            {"interval", 1}});
                for (int i = page * 64; i < std::min(count, (page + 1) * 64); ++i)
                    addRow(plugin["parameters"][i]);
            }
        }
        std::map<std::string, const Json*> values;
        if (!plugin.is_null())
            for (const auto& p : plugin["parameters"])
                values[p["id"]] = &p;
        if (!plugin.is_null())
            for (auto& row : rows)
            {
                Json p;
                if (row->id == "@delay_time")
                    p = {{"value", plugin["delay_time_ms"]},
                         {"display", std::to_string(plugin["delay_time_ms"].get<int>()) + " ms"}};
                else if (values.contains(row->id))
                    p = *values.at(row->id);
                if (p.is_null())
                    continue;
                row->value = playing ? p.value("current_value", p["value"].get<double>()) : p["value"].get<double>();
                row->display.setText(text(p["display"].get<std::string>()), juce::dontSendNotification);
                row->slider.setTooltip(text("参数 ID: ") + text(row->id) +
                                       text(plugin.contains("owner_clip")
                                                ? " · 片段参数，停止后可编辑；手势整体撤销；片段自动化尚未验收"
                                                : " · 拖动实时调参，一次撤销整个手势；播放时按自动化模式录写"));
                if (!row->gesture && !row->slider.isMouseButtonDown())
                    row->slider.setValue(row->value, juce::dontSendNotification);
                row->slider.setEnabled(!playing || (liveWrite && row->id != "@delay_time"));
                if (row->gesture &&
                    ((row->automationGesture && !playing) || (!row->automationGesture && !nativeActive)))
                {
                    row->gesture = false;
                    row->stoppedGesture = true;
                }
            }
        for (auto* c : std::initializer_list<juce::Component*>{&previous, &next, &pageLabel})
            c->setVisible(paged);
        previous.setEnabled(page > 0 && !nativeActive && !playing);
        next.setEnabled(page + 1 < pages && !nativeActive && !playing);
        pageLabel.setText(juce::String(page + 1) + "/" + juce::String(pages) + " \u00b7 " + juce::String(count),
                          juce::dontSendNotification);
        setSize(width, std::max(40, int(rows.size()) * 70 + (paged ? 36 : 0)));
        resized();
    }
    void resized() override
    {
        previous.setBounds(8, 2, 66, 28);
        next.setBounds(getWidth() - 74, 2, 66, 28);
        pageLabel.setBounds(80, 2, getWidth() - 160, 28);
        int y = paged ? 36 : 0;
        for (auto& row : rows)
        {
            row->name.setBounds(10, y + 4, getWidth() - 125, 22);
            row->display.setBounds(getWidth() - 115, y + 4, 105, 22);
            row->slider.setBounds(8, y + 28, getWidth() - 16, 30);
            y += 70;
        }
    }

private:
    void refreshPage()
    {
        rowSignature.clear();
        update(lastPlugin, lastPlaying, lastWidth, lastTrack, lastWriting, lastMode, lastNativeActive);
    }
    struct Row
    {
        std::string id;
        double value = 0;
        bool gesture = false, stoppedGesture = false, automationGesture = false;
        juce::Label name, display;
        juce::Slider slider;
    };
    void control(Row& row, const char* action)
    {
        if (!row.automationGesture)
        {
            Json args = {{"plugin", pluginID}, {"parameter", row.id}};
            if (std::string(action) == "value")
                args["value"] = row.slider.getValue();
            write(std::string("parameter.gesture.") + action, args);
            return;
        }
        Json args = {{"track", trackID}, {"parameter", pluginID + "::" + row.id}};
        if (std::string(action) == "value")
            args["value"] = row.slider.getValue();
        write(std::string("automation.gesture.") + action, args);
    }
    void addRow(const Json& p)
    {
        auto row = std::make_unique<Row>();
        row->id = p["id"];
        row->name.setText(text(p["name"].get<std::string>()), juce::dontSendNotification);
        row->name.setFont(juce::FontOptions(12));
        row->display.setFont(juce::FontOptions(12));
        row->display.setJustificationType(juce::Justification::right);
        row->slider.setSliderStyle(juce::Slider::LinearHorizontal);
        row->slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 24);
        row->slider.setRange(p["minimum"], p["maximum"], p.value("interval", 0.));
        row->slider.setNumDecimalPlacesToDisplay(p["maximum"].get<double>() > 1000 ? 0 : 3);
        double lo = p["minimum"], hi = p["maximum"];
        if (lo > 0 && hi / lo > 50)
            row->slider.setSkewFactorFromMidPoint(std::sqrt(lo * hi));
        row->slider.setTooltip(text("参数 ID: ") + text(row->id) +
                               text(" · 拖动实时调参，一次撤销整个手势；播放时按自动化模式录写"));
        row->slider.setComponentID("plugin.parameter:" + text(row->id));
        auto* ptr = row.get();
        auto change = [this, ptr]
        {
            if (std::abs(ptr->slider.getValue() - ptr->value) < 1e-6)
                return;
            if (ptr->id == "@delay_time")
                write("plugin.delay_time", {{"plugin", pluginID}, {"ms", std::llround(ptr->slider.getValue())}});
            else
                write("parameter.gesture.value",
                      {{"plugin", pluginID}, {"parameter", ptr->id}, {"value", ptr->slider.getValue()}});
        };
        row->slider.onDragStart = [this, ptr]
        {
            ptr->stoppedGesture = false;
            if ((liveWrite || stopped) && ptr->id != "@delay_time")
            {
                ptr->automationGesture = liveWrite;
                ptr->gesture = true;
                control(*ptr, "begin");
            }
        };
        row->slider.onDragEnd = [this, ptr, change]
        {
            if (ptr->stoppedGesture)
            {
                ptr->stoppedGesture = false;
                return;
            }
            if (ptr->gesture)
            {
                control(*ptr, "end");
                ptr->gesture = false;
            }
            else
                change();
        };
        row->slider.onValueChange = [this, ptr, change]
        {
            if (ptr->gesture)
                control(*ptr, "value");
            else if (!ptr->slider.isMouseButtonDown())
            {
                if (liveWrite && ptr->id != "@delay_time")
                {
                    ptr->automationGesture = true;
                    control(*ptr, "begin");
                    control(*ptr, "value");
                    control(*ptr, "end");
                }
                else
                    change();
            }
        };
        for (auto* c : std::initializer_list<juce::Component*>{&row->name, &row->display, &row->slider})
            addAndMakeVisible(c);
        rows.push_back(std::move(row));
    }
    Writer write;
    std::string pluginID, trackID, rowSignature;
    bool liveWrite = false, stopped = true, paged = false;
    int page = 0;
    Json lastPlugin = nullptr;
    bool lastPlaying = false, lastWriting = false, lastNativeActive = false;
    int lastWidth = 300;
    std::string lastTrack, lastMode;
    juce::TextButton previous{text("\u4e0a\u4e00\u9875")}, next{text("\u4e0b\u4e00\u9875")};
    juce::Label pageLabel;
    std::vector<std::unique_ptr<Row>> rows;
};

} // namespace ndaw::desktop
