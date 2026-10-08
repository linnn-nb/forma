#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class ClipsList final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit ClipsList(std::function<void(std::string)> select) : select(std::move(select))
    {
        setComponentID("clips.list");
        addAndMakeVisible(list);
        list.setModel(this);
        list.setRowHeight(25);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff242831));
    }
    void update(const Json& tracks)
    {
        Json next = Json::array();
        for (const auto& t : tracks)
            for (const auto& c : t["clips"])
                next.push_back({{"id", c["id"]}, {"name", c["name"]}, {"kind", c["kind"]}, {"track", t["id"]}});
        if (next != rows)
        {
            rows = next;
            list.updateContent();
        }
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff242831));
        g.setColour(juce::Colour(0xffa6afbd));
        g.setFont(juce::FontOptions(11, juce::Font::bold));
        g.drawText(text("CLIPS · 片段 ") + juce::String(rows.size()), 12, 3, getWidth() - 20, 28,
                   juce::Justification::left);
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
        g.setColour(juce::Colour(0xffcbd3de));
        g.setFont(juce::FontOptions(11));
        g.drawText((rows[i]["kind"] == "midi" ? "M  " : "~  ") + text(rows[i]["name"].get<std::string>()), 8, 0, w - 12,
                   h, juce::Justification::left);
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
