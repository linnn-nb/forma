#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class TracksList final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit TracksList(std::function<void(std::string)> select) : select(std::move(select))
    {
        setComponentID("tracks.list");
        addAndMakeVisible(list);
        list.setModel(this);
        list.setRowHeight(26);
        list.setMultipleSelectionEnabled(true);
        list.setColour(juce::ListBox::backgroundColourId, base().darker(.15f));
    }
    void update(const Json& tracks, const std::string& selected, const Json& selection = Json::array())
    {
        Json next = Json::array();
        for (const auto& t : tracks)
            next.push_back({{"id", t["id"]},
                            {"name", t["name"]},
                            {"colour", t.value("colour", Json(nullptr))},
                            {"type", t["type"]},
                            {"hidden", t.value("edit_hidden", false)}});
        if (next != rows)
        {
            rows = next;
            list.updateContent();
        }
        list.deselectAllRows();
        for (int i = 0; i < int(rows.size()); ++i)
            if (rows[i]["id"] == selected ||
                std::find(selection.begin(), selection.end(), rows[i]["id"]) != selection.end())
                list.selectRow(i, true, false);
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base().darker(.15f));
        g.setColour(juce::Colour(0xffa6afbd));
        g.setFont(juce::FontOptions(11, juce::Font::bold));
        g.drawText(text("TRACKS · 轨道"), 12, 3, getWidth() - 20, 28, juce::Justification::left);
    }
    void resized() override
    {
        list.setBounds(4, 34, getWidth() - 8, getHeight() - 38);
    }

private:
    int getNumRows() override
    {
        return int(rows.size());
    }
    void paintListBoxItem(int i, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (i < 0 || i >= int(rows.size()))
            return;
        if (selected)
        {
            g.setColour(juce::Colour(0xff353c47));
            g.fillAll();
        }
        g.setColour(trackColour(rows[i]));
        g.fillRect(4, 7, 3, h - 14);
        g.setFont(juce::FontOptions(11));
        g.setColour(juce::Colour(0xffcbd3de));
        g.drawText(text(rows[i]["name"].get<std::string>()), 12, 0, w - 16, h, juce::Justification::left);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < int(rows.size()))
            select(rows[row]["id"]);
    }
    std::function<void(std::string)> select;
    Json rows = Json::array();
    juce::ListBox list;
};
} // namespace ndaw::desktop
