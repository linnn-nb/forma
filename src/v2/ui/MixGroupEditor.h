#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class MixGroupEditor final : public juce::Component
{
public:
    using Write = std::function<void(const std::string&, const Json&, const Json&)>;
    MixGroupEditor(Write write, std::function<void()> close) : write(std::move(write)), close(std::move(close))
    {
        setComponentID("mix.group.editor");
        for (auto* c : std::initializer_list<juce::Component*>{&title, &name, &enabled, &mute, &solo, &editing, &view,
                                                               &apply, &remove, &cancel, &status})
            addAndMakeVisible(c);
        title.setText(text("Track Group · 编辑 / 混音"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        name.setComponentID("mix.group.name");
        name.setInputRestrictions(64);
        enabled.setButtonText(text("启用组"));
        mute.setButtonText(text("Main Mute 联动"));
        solo.setButtonText(text("Solo 联动"));
        enabled.setComponentID("mix.group.enabled");
        mute.setComponentID("mix.group.mute");
        solo.setComponentID("mix.group.solo");
        editing.setButtonText(text("Edit · 联动编辑"));
        editing.setComponentID("mix.group.edit");
        apply.setButtonText(text("应用 · 一次 Undo"));
        remove.setButtonText(text("删除组"));
        cancel.setButtonText(text("取消"));
        apply.setComponentID("mix.group.apply");
        remove.setComponentID("mix.group.delete");
        cancel.setComponentID("mix.group.cancel");
        status.setComponentID("mix.group.status");
        status.setText(text("选择成员；只联动已勾选的 Mute / Solo，原输出与 Folder/VCA 层级保持。"),
                       juce::dontSendNotification);
        view.setViewedComponent(&body, false);
        view.setScrollBarsShown(true, false);
        apply.onClick = [this]
        {
            Json members = Json::array();
            for (size_t i = 0; i < buttons.size(); ++i)
                if (buttons[i]->getToggleState())
                    members.push_back(ids[i]);
            execute(creating ? "group.create" : "group.update", {{"id", id},
                                                                 {"name", name.getText().toStdString()},
                                                                 {"members", members},
                                                                 {"enabled", enabled.getToggleState()},
                                                                 {"mute", mute.getToggleState()},
                                                                 {"solo", solo.getToggleState()},
                                                                 {"edit", editing.getToggleState()}});
        };
        remove.onClick = [this] { execute("group.delete", {{"id", id}}); };
        cancel.onClick = [this] { this->close(); };
    }
    void bind(const Json& facts, const std::string& existing, const Json& selection)
    {
        binding = {{"session_token", facts["session_token"]}, {"base_revision", facts["revision"]}};
        creating = existing.empty();
        id = creating ? juce::Uuid().toString().toStdString() : existing;
        Json group{{"name", "Mix Group"}, {"members", selection}, {"enabled", true}, {"mute", true}, {"solo", true}};
        if (!creating)
            for (const auto& candidate : facts["mix_groups"])
                if (candidate["id"] == id)
                    group = candidate;
        name.setText(text(group["name"].get<std::string>()), false);
        enabled.setToggleState(group["enabled"], juce::dontSendNotification);
        mute.setToggleState(group["mute"], juce::dontSendNotification);
        solo.setToggleState(group["solo"], juce::dontSendNotification);
        editing.setToggleState(group.value("edit", false), juce::dontSendNotification);
        remove.setVisible(!creating);
        buttons.clear();
        ids.clear();
        for (const auto& track : facts["tracks"])
            if (track["capabilities"]["audio_routing"].get<bool>())
            {
                const auto target = track["id"].get<std::string>();
                auto b = std::make_unique<juce::ToggleButton>(text(track["name"].get<std::string>()));
                b->setComponentID("mix.group.member:" + text(target));
                b->setToggleState(std::find(group["members"].begin(), group["members"].end(), Json(target)) !=
                                      group["members"].end(),
                                  juce::dontSendNotification);
                body.addAndMakeVisible(*b);
                ids.push_back(target);
                buttons.push_back(std::move(b));
            }
        status.setText(text("至少2个成员。Edit联动选区、移动、修剪、淡化和片段增益；推子、Pan和MIDI联动待实现。"),
                       juce::dontSendNotification);
        resized();
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
    }
    void resized() override
    {
        const int width = std::min(680, getWidth() - 40), x = (getWidth() - width) / 2;
        title.setBounds(x, 24, width, 32);
        name.setBounds(x, 68, width, 30);
        enabled.setBounds(x, 110, 110, 28);
        mute.setBounds(x + 120, 110, 180, 28);
        solo.setBounds(x + 310, 110, 160, 28);
        editing.setBounds(x + 480, 110, 200, 28);
        view.setBounds(x, 153, width, std::max(40, getHeight() - 277));
        body.setSize(width - 16, std::max(1, int(buttons.size()) * 30));
        for (size_t i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds(4, int(i) * 30, width - 24, 28);
        status.setBounds(x, getHeight() - 116, width, 48);
        apply.setBounds(x, getHeight() - 56, 176, 30);
        remove.setBounds(x + 188, getHeight() - 56, 112, 30);
        cancel.setBounds(x + width - 112, getHeight() - 56, 112, 30);
    }

private:
    void execute(const std::string& command, const Json& args)
    {
        try
        {
            write(command, args, binding);
            close();
        }
        catch (const std::exception& error)
        {
            status.setText(text(error.what()), juce::dontSendNotification);
        }
    }
    Write write;
    std::function<void()> close;
    Json binding;
    std::string id;
    bool creating = true;
    juce::Label title, status;
    juce::TextEditor name;
    juce::ToggleButton enabled, mute, solo, editing;
    juce::Viewport view;
    juce::Component body;
    std::vector<std::string> ids;
    std::vector<std::unique_ptr<juce::ToggleButton>> buttons;
    juce::TextButton apply, remove, cancel;
};
} // namespace ndaw::desktop
