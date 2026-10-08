#pragma once
#include "Theme.h"

namespace ndaw::desktop
{
class MemoryLocationsPanel final : public juce::Component, private juce::ListBoxModel, private juce::KeyListener
{
public:
    using Write = std::function<void(const std::string&, const Json&, const Json&)>;
    using Seek = std::function<void(int64_t, const Json&)>;

    MemoryLocationsPanel(Write write, Seek seek, std::function<void()> close)
        : write(std::move(write)), seek(std::move(seek)), close(std::move(close)), list("memory-locations", this)
    {
        setComponentID("memory.locations.panel");
        for (auto* component :
             std::initializer_list<juce::Component*>{&title, &list, &name, &addMarker, &storeSelection, &goTo, &rename,
                                                     &moveToPlayhead, &remove, &closeButton, &detail, &status})
            addAndMakeVisible(component);
        title.setText(text("Memory Locations"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(23, juce::Font::bold));
        name.setComponentID("memory.locations.name");
        name.setTextToShowWhenEmpty(text("位置名称"), juce::Colour(0xff8293a4));
        name.setInputRestrictions(96);
        name.addKeyListener(this);
        list.addKeyListener(this);
        list.setComponentID("memory.locations.list");
        list.setRowHeight(46);
        addMarker.setButtonText(text("标记当前位置"));
        storeSelection.setButtonText(text("保存当前选区"));
        goTo.setButtonText(text("定位 / 恢复选区"));
        rename.setButtonText(text("重命名"));
        moveToPlayhead.setButtonText(text("移到播放位置"));
        remove.setButtonText(text("删除"));
        closeButton.setButtonText(text("关闭"));
        detail.setComponentID("memory.locations.detail");
        status.setComponentID("memory.locations.status");
        addMarker.setComponentID("memory.locations.add");
        storeSelection.setComponentID("memory.locations.store_selection");
        goTo.setComponentID("memory.locations.go");
        rename.setComponentID("memory.locations.rename");
        moveToPlayhead.setComponentID("memory.locations.move");
        remove.setComponentID("memory.locations.delete");
        closeButton.setComponentID("memory.locations.close");
        detail.setMultiLine(true);
        detail.setReadOnly(true);
        addMarker.onClick = [this]
        {
            Json args = Json::object();
            if (const auto entered = name.getText().trim(); entered.isNotEmpty())
                args["name"] = entered.toStdString();
            execute("marker.create", args);
        };
        storeSelection.onClick = [this]
        {
            Json args = Json::object();
            if (const auto entered = name.getText().trim(); entered.isNotEmpty())
                args["name"] = entered.toStdString();
            execute("location.store_selection", args);
        };
        goTo.onClick = [this]
        {
            try
            {
                const auto* marker = selected();
                if (!marker)
                    throw std::runtime_error("先选择一个位置");
                const auto location = *marker;
                const auto position = location.at("position_samples").get<int64_t>();
                if (location.value("kind", std::string{}) == "selection")
                    execute("session.range.set",
                            {{"start_samples", position},
                             {"end_samples", position + location.at("length_samples").get<int64_t>()}});
                this->seek(position, binding);
                status.setText(text(location.value("kind", std::string{}) == "selection"
                                        ? "已恢复选区并定位。"
                                        : "已定位到 Memory Location。"),
                               juce::dontSendNotification);
            }
            catch (const std::exception& e)
            {
                status.setText(text("未执行：") + text(e.what()), juce::dontSendNotification);
            }
        };
        rename.onClick = [this]
        {
            const auto* marker = selected();
            if (marker)
                execute("marker.rename", {{"marker", marker->at("id")}, {"name", name.getText().trim().toStdString()}});
        };
        moveToPlayhead.onClick = [this]
        {
            const auto* marker = selected();
            if (marker)
                execute("marker.move",
                        {{"marker", marker->at("id")}, {"position_samples", facts.at("position_samples")}});
        };
        remove.onClick = [this]
        {
            const auto* marker = selected();
            if (marker)
                execute("marker.delete", {{"marker", marker->at("id")}});
        };
        closeButton.onClick = [this] { this->close(); };
    }

    void bind(const Json& value)
    {
        const auto selectedBefore = selectedID;
        facts = value;
        binding = {{"session_token", value["session_token"]}, {"base_revision", value["revision"]}};
        markers = value.value("markers", Json::array());
        list.updateContent();
        const auto found = std::find_if(markers.begin(), markers.end(), [&](const auto& marker)
                                        { return marker.value("id", std::string{}) == selectedID; });
        if (found == markers.end())
            selectedID.clear();
        if (selectedID.empty() && !markers.empty())
            selectedID = markers[0]["id"].get<std::string>();
        const auto selectedRow = std::find_if(markers.begin(), markers.end(), [&](const auto& marker)
                                              { return marker.value("id", std::string{}) == selectedID; });
        list.selectRow(selectedRow == markers.end() ? -1 : int(std::distance(markers.begin(), selectedRow)),
                       juce::dontSendNotification);
        if (selectedID != selectedBefore)
            loadSelectedName();
        updateControls();
    }

    void update(const Json& value)
    {
        const auto previous = binding;
        const auto selectedBefore = selectedID;
        bind(value);
        if (selectedID != selectedBefore ||
            previous.value("session_token", std::string{}) != binding.value("session_token", std::string{}))
            loadSelectedName();
    }

    bool selectID(const std::string& id)
    {
        const auto found = std::find_if(markers.begin(), markers.end(),
                                        [&](const auto& marker) { return marker.value("id", std::string{}) == id; });
        if (found == markers.end())
            return false;
        selectedID = id;
        const auto row = int(std::distance(markers.begin(), found));
        list.selectRow(row, juce::dontSendNotification);
        loadSelectedName();
        updateControls();
        return true;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(.78f));
        g.setColour(base());
        g.fillRoundedRectangle(card.toFloat(), 8);
        g.setColour(juce::Colour(0xff45505e));
        g.drawRoundedRectangle(card.toFloat(), 8, 1);
    }

    void resized() override
    {
        const auto preferredHeight = std::clamp(260 + int(markers.size()) * 46, 380, 560);
        card = juce::Rectangle<int>(std::min(930, getWidth() - 24), std::min(preferredHeight, getHeight() - 24))
                   .withCentre(getLocalBounds().getCentre());
        const int x = card.getX() + 22, y = card.getY() + 18, w = card.getWidth() - 44;
        title.setBounds(x, y, w - 210, 34);
        name.setBounds(x + w - 202, y + 2, 202, 30);
        list.setBounds(x, y + 46, w, std::max(150, card.getHeight() - 220));
        detail.setBounds(x, list.getBottom() + 8, w, 38);
        const int buttonY = card.getBottom() - 96;
        const int buttonW = (w - 36) / 4;
        addMarker.setBounds(x, buttonY, buttonW, 30);
        storeSelection.setBounds(x + buttonW + 12, buttonY, buttonW, 30);
        goTo.setBounds(x + (buttonW + 12) * 2, buttonY, buttonW, 30);
        rename.setBounds(x + (buttonW + 12) * 3, buttonY, buttonW, 30);
        const int secondY = buttonY + 38, half = (w - 24) / 3;
        moveToPlayhead.setBounds(x, secondY, half, 30);
        remove.setBounds(x + half + 12, secondY, half, 30);
        closeButton.setBounds(x + (half + 12) * 2, secondY, half, 30);
        status.setBounds(x, card.getBottom() - 34, w, 24);
    }

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }
        return false;
    }

    int getNumRows() override
    {
        return int(markers.size());
    }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selectedRow) override
    {
        if (row < 0 || row >= int(markers.size()))
            return;
        g.fillAll(selectedRow ? juce::Colour(0xff29444a) : juce::Colour(0xff1a222c));
        const auto& item = markers[size_t(row)];
        g.setColour(selectedRow ? accent() : juce::Colour(0xffd7e0e9));
        g.setFont(juce::FontOptions(14, juce::Font::bold));
        g.drawText(text(item.value("name", std::string{})), 12, 4, width - 24, 20, juce::Justification::left);
        g.setColour(juce::Colour(0xff9cadbc));
        const auto kind = item.value("kind", std::string{}) == "selection" ? text("选区") : text("标记");
        const auto location = text("小节 ") + juce::String(item.value("bar", 1)) + " · " +
                              juce::String(item.value("beat", 1.0), 2) + " 拍 · " +
                              text(std::to_string(item.value("position_samples", int64_t(0)))) + text(" 样本");
        g.setFont(juce::FontOptions(11));
        g.drawText(kind + text("  ·  ") + location, 12, 25, width - 24, height - 26, juce::Justification::left);
    }

    void selectedRowsChanged(int row) override
    {
        if (row < 0 || row >= int(markers.size()))
        {
            selectedID.clear();
            updateControls();
            return;
        }
        selectedID = markers[size_t(row)]["id"].get<std::string>();
        loadSelectedName();
        updateControls();
    }

    const Json* selected() const
    {
        auto found = std::find_if(markers.begin(), markers.end(),
                                  [&](const auto& marker) { return marker.value("id", std::string{}) == selectedID; });
        return found == markers.end() ? nullptr : &*found;
    }

    void loadSelectedName()
    {
        if (const auto* marker = selected())
            name.setText(text(marker->value("name", std::string{})), false);
    }

    void updateControls()
    {
        const auto* marker = selected();
        const bool stopped = !facts.value("playing", false) && !facts.value("recording", false) &&
                             facts.value("recording_capture", Json(nullptr)).is_null() &&
                             facts.value("parameter_capture", Json(nullptr)).is_null() &&
                             facts.value("automation_capture", Json(nullptr)).is_null();
        addMarker.setEnabled(stopped);
        storeSelection.setEnabled(stopped && !facts.value("time_selection", Json(nullptr)).is_null());
        goTo.setEnabled(marker != nullptr && stopped);
        rename.setEnabled(marker != nullptr && stopped);
        moveToPlayhead.setEnabled(marker != nullptr && stopped);
        remove.setEnabled(marker != nullptr && stopped);
        detail.setText(markers.empty() ? text("可在时间线上按 M 添加标记；选中时间范围后可保存为 Memory Location。")
                                       : text(std::to_string(markers.size())) +
                                             text(" 个位置 · 修改进入工程 Undo 历史并随工程保存。"));
    }

    void execute(const std::string& command, const Json& args)
    {
        try
        {
            write(command, args, binding);
            status.setText(text("已提交 · 可撤销"), juce::dontSendNotification);
        }
        catch (const std::exception& e)
        {
            status.setText(text("未执行：") + text(e.what()), juce::dontSendNotification);
        }
    }

    Write write;
    Seek seek;
    std::function<void()> close;
    Json facts = Json::object(), binding = Json::object(), markers = Json::array();
    std::string selectedID;
    juce::Rectangle<int> card;
    juce::Label title, status;
    juce::TextEditor detail;
    juce::ListBox list;
    juce::TextEditor name;
    juce::TextButton addMarker{text("标记当前位置")}, storeSelection{text("保存当前选区")},
        goTo{text("定位 / 恢复选区")}, rename{text("重命名")}, moveToPlayhead{text("移到播放位置")},
        remove{text("删除")}, closeButton{text("关闭")};
};
} // namespace ndaw::desktop
