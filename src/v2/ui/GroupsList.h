#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class GroupsList final : public juce::Component
{
public:
    using Select = std::function<void(const std::string&)>;
    GroupsList(Select select, std::function<void(const std::string&, bool)> enable, Select edit)
        : select(std::move(select)), enable(std::move(enable)), edit(std::move(edit))
    {
        setComponentID("groups.list");
        addAndMakeVisible(view);
        view.setViewedComponent(&body, false);
        view.setScrollBarsShown(true, false);
        addAndMakeVisible(create);
        create.setButtonText("+");
        create.setComponentID("groups.create");
        create.setTooltip(text("新建编辑 / 混音组 · ⌘G"));
        create.onClick = [this] { this->edit(""); };
    }
    void update(const Json& groups, const Json& tracks, const Json& selection, bool playing)
    {
        std::vector<std::string> ids;
        for (const auto& group : groups)
            ids.push_back(group["id"]);
        if (ids != rowIDs)
        {
            rowIDs = ids;
            rows.clear();
            for (const auto& id : ids)
            {
                auto row = std::make_unique<Row>(id, select, enable, edit);
                body.addAndMakeVisible(*row);
                rows.push_back(std::move(row));
            }
        }
        std::set<std::string> selectedTracks;
        for (const auto& id : selection)
            selectedTracks.insert(id.get<std::string>());
        selected.clear();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            std::set<std::string> members;
            for (const auto& member : groups[i]["members"])
                members.insert(member.get<std::string>());
            const bool chosen = members == selectedTracks;
            if (chosen && selected.empty())
                selected = rowIDs[i];
            if (chosen && rowIDs[i] == preferred)
                selected = preferred;
        }
        for (size_t i = 0; i < rows.size(); ++i)
            rows[i]->update(groups[i], tracks, rowIDs[i] == selected, playing);
        create.setEnabled(!playing);
        resized();
        repaint();
    }
    void preferSelection(const std::string& id)
    {
        preferred = id;
    }
    std::string selectedId() const
    {
        return selected;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base().darker(.15f));
        g.setColour(juce::Colour(0xffa6afbd));
        g.setFont(juce::FontOptions(11, juce::Font::bold));
        g.drawText("GROUPS", 10, 0, 86, 28, juce::Justification::centredLeft);
        if (rows.empty())
        {
            g.setFont(juce::FontOptions(10));
            g.drawText(text("编辑 / 混音组"), 8, 38, getWidth() - 16, 22, juce::Justification::centredLeft);
            g.drawText(text("Edit / Mute / Solo"), 8, 61, getWidth() - 16, 22, juce::Justification::centredLeft);
        }
    }
    void resized() override
    {
        create.setBounds(getWidth() - 31, 3, 25, 23);
        view.setBounds(0, 32, getWidth(), std::max(1, getHeight() - 32));
        const int width = std::max(1, getWidth() - 14);
        body.setSize(width, std::max(1, int(rows.size()) * 36));
        for (size_t i = 0; i < rows.size(); ++i)
            rows[i]->setBounds(0, int(i) * 36, width, 34);
    }

private:
    class Row final : public juce::Component
    {
    public:
        Row(const std::string& id, Select select, std::function<void(const std::string&, bool)> enable, Select edit)
        {
            for (auto* b : std::initializer_list<juce::Button*>{&active, &name, &settings})
                addAndMakeVisible(*b);
            active.setComponentID("groups.enabled:" + text(id));
            name.setComponentID("groups.select:" + text(id));
            settings.setComponentID("groups.edit:" + text(id));
            active.setClickingTogglesState(false);
            active.onClick = [this, id, enable] { enable(id, !active.getToggleState()); };
            name.onClick = [id, select] { select(id); };
            settings.onClick = [id, edit] { edit(id); };
            settings.setButtonText(text("…"));
        }
        void update(const Json& group, const Json& tracks, bool selected, bool playing)
        {
            active.setToggleState(group["enabled"], juce::dontSendNotification);
            active.setEnabled(!playing);
            settings.setEnabled(!playing);
            name.setButtonText(text(group["name"].get<std::string>()));
            name.setToggleState(selected, juce::dontSendNotification);
            auto tooltip = text(group["type"].get<std::string>()) + " · " + (group["mute"].get<bool>() ? "M " : "") +
                           (group["solo"].get<bool>() ? "S" : "");
            for (const auto& member : group["members"])
            {
                juce::String label = text("缺失 ") + text(member.get<std::string>());
                for (const auto& track : tracks)
                    if (track["id"] == member)
                        label = text(track["name"].get<std::string>());
                tooltip += "\n" + label;
            }
            name.setTooltip(tooltip + text("\n点名称选择成员；勾选启用联动，…修改组"));
            active.setTooltip(text("启用 / 禁用该组 · 不改变输出路由"));
            settings.setTooltip(text("修改组名、成员与联动属性"));
        }
        void resized() override
        {
            active.setBounds(2, 3, 22, 28);
            name.setBounds(24, 3, std::max(1, getWidth() - 45), 28);
            settings.setBounds(getWidth() - 20, 3, 19, 28);
        }

    private:
        juce::ToggleButton active;
        juce::TextButton name, settings;
    };
    Select select, edit;
    std::function<void(const std::string&, bool)> enable;
    juce::Viewport view;
    juce::Component body;
    juce::TextButton create;
    std::string selected, preferred;
    std::vector<std::string> rowIDs;
    std::vector<std::unique_ptr<Row>> rows;
};
} // namespace ndaw::desktop
