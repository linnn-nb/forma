#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// Native track-row controls read query facts; actions reuse the workspace command/inspector path.
class EditWindowViews final : public juce::Component
{
public:
    EditWindowViews(std::string owner, std::function<void(std::string, int)> insert,
                    std::function<void(std::string, bool)> route,
                    std::function<void(std::string, std::string)> send = {},
                    std::function<void(std::string)> comment = {})
        : owner(std::move(owner)), insert(std::move(insert)), route(std::move(route)), send(std::move(send)),
          comment(std::move(comment))
    {
        setComponentID("edit.views:" + text(this->owner));
        comments.setComponentID("edit.comment:" + text(this->owner));
        addAndMakeVisible(comments);
        comments.onClick = [this]
        {
            if (this->comment)
                this->comment(this->owner);
        };
        input.setComponentID("edit.input:" + text(this->owner));
        output.setComponentID("edit.output:" + text(this->owner));
        for (auto* b : {&input, &output})
            addAndMakeVisible(*b);
        input.onClick = [this]
        {
            if (this->route)
                this->route(this->owner, true);
        };
        output.onClick = [this]
        {
            if (this->route)
                this->route(this->owner, false);
        };
        for (int i = 0; i < 5; ++i)
        {
            auto a = std::make_unique<juce::TextButton>();
            a->setComponentID("edit.insert:" + text(this->owner) + ":" + juce::String(i));
            a->onClick = [this, i]
            {
                if (this->insert)
                    this->insert(this->owner, i);
            };
            addAndMakeVisible(*a);
            inserts.push_back(std::move(a));
            auto s = std::make_unique<juce::TextButton>();
            s->setComponentID("edit.send:" + text(this->owner) + ":" + juce::String(i));
            s->onClick = [this, i]
            {
                const auto items = facts.value("sends", Json::array());
                if (i < int(items.size()) && this->send)
                    this->send(this->owner, items[i]["id"].get<std::string>());
                else if (this->route)
                    this->route(this->owner, false);
            };
            addAndMakeVisible(*s);
            sends.push_back(std::move(s));
        }
    }
    void update(const Json& track, const Json& tracks)
    {
        facts = track;
        const bool capable = facts["capabilities"].value("audio_routing", false);
        input.setButtonText(text(facts.value("input", Json::object()).value("name", std::string("—"))));
        output.setButtonText(text(facts.value("output", Json::object()).value("name", std::string("—"))));
        input.setEnabled(capable && bool(route));
        output.setEnabled(capable && bool(route));
        input.setTooltip(text("输入 / 待命 / 监听：打开录音检查器"));
        output.setTooltip(text("输出路由：打开 I/O 检查器"));
        const auto plugins = facts.value("plugins", Json::array()), sendFacts = facts.value("sends", Json::array());
        for (int i = 0; i < 5; ++i)
        {
            const auto letter = juce::String::charToString(juce::juce_wchar('A' + i));
            inserts[i]->setButtonText(i < int(plugins.size()) ? text(plugins[i]["name"].get<std::string>())
                                                              : letter + "  +");
            inserts[i]->setTooltip(i < int(plugins.size())
                                       ? letter + " · " + text(plugins[i]["name"].get<std::string>())
                                       : letter + text(" · 插入效果器"));
            inserts[i]->setEnabled(capable && bool(insert));
            auto name = letter + "  +", tooltip = text("打开发送检查器，创建或编辑真实发送");
            if (i < int(sendFacts.size()))
            {
                const auto& s = sendFacts[i];
                name = text("未解析 Bus ") + juce::String(s["bus"].get<int>());
                for (const auto& t : tracks)
                    if (s["target"] == t["id"])
                        name = text(t["name"].get<std::string>());
                tooltip = name + " · " + text(s["position"].get<std::string>()) + " · " +
                          juce::String(s["db"].get<double>(), 1) + " dB";
                if (!s["enabled"].get<bool>())
                    name = text("旁通 · ") + name;
            }
            sends[i]->setButtonText(name);
            sends[i]->setTooltip(tooltip);
            sends[i]->setEnabled(capable && bool(route));
        }
        const auto value = text(facts.value("comment", std::string{}));
        comments.setButtonText(value.isEmpty() ? text("添加备注…") : value.upToFirstOccurrenceOf("\n", false, false));
        comments.setTooltip(value.isEmpty() ? text("编辑轨道备注") : value);
        comments.setEnabled(bool(comment) && !facts.value("playing", false));
        repaint();
    }
    void configure(const Json& views, int width)
    {
        enabled = views;
        columnWidth = std::max(48, width);
        resized();
        repaint();
    }
    void resized() override
    {
        int x = 0;
        for (const auto* key : {"io", "inserts", "sends", "comments"})
        {
            const bool show = enabled.value(key, false);
            if (std::string(key) == "io")
            {
                input.setVisible(show);
                output.setVisible(show);
                input.setBounds(x + 4, 24, columnWidth - 8, 22);
                output.setBounds(x + 4, 50, columnWidth - 8, 22);
            }
            else if (std::string(key) == "comments")
            {
                comments.setVisible(show);
                comments.setBounds(x + 4, 24, columnWidth - 8, std::max(22, getHeight() - 28));
            }
            else
            {
                auto& buttons = std::string(key) == "inserts" ? inserts : sends;
                const int step = std::max(12, std::min(22, (getHeight() - 28) / 5));
                for (int i = 0; i < 5; ++i)
                {
                    buttons[i]->setVisible(show);
                    buttons[i]->setBounds(x + 4, 24 + i * step, columnWidth - 8, step - 2);
                }
            }
            if (show)
                x += columnWidth;
        }
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff28333e));
        g.setColour(juce::Colour(0xff91a5b7));
        g.setFont(juce::FontOptions(10));
        int x = 0;
        for (const auto* key : {"io", "inserts", "sends", "comments"})
            if (enabled.value(key, false))
            {
                g.drawText(text(std::string(key) == "io"        ? "I/O"
                                : std::string(key) == "inserts" ? "INSERTS A–E"
                                : std::string(key) == "sends"   ? "SENDS A–E"
                                                                : "COMMENTS"),
                           x + 4, 2, columnWidth - 8, 20, juce::Justification::centredLeft);
                g.drawVerticalLine(x + columnWidth - 1, 0, float(getHeight()));
                x += columnWidth;
            }
    }

private:
    std::string owner;
    std::function<void(std::string, int)> insert;
    std::function<void(std::string, bool)> route;
    std::function<void(std::string, std::string)> send;
    std::function<void(std::string)> comment;
    Json facts = Json::object(), enabled = Json::object();
    int columnWidth = 100;
    juce::TextButton input, output, comments;
    std::vector<std::unique_ptr<juce::TextButton>> inserts, sends;
};
} // namespace ndaw::desktop
