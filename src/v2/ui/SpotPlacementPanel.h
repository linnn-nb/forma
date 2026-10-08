#pragma once
#include "Theme.h"

namespace ndaw::desktop
{
class SpotPlacementPanel final : public juce::Component, private juce::KeyListener
{
public:
    using Place = std::function<void(const std::string&, int, double, const Json&)>;

    SpotPlacementPanel(Place place, std::function<void()> close) : place(std::move(place)), close(std::move(close))
    {
        setComponentID("edit.spot.panel");
        for (juce::Component* component : std::array<juce::Component*, 10>{
                 &title, &clipName, &barLabel, &beatLabel, &bar, &beat, &hint, &status, &apply, &cancel})
            addAndMakeVisible(component);
        title.setText(text("Spot Placement"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        clipName.setComponentID("edit.spot.clip");
        barLabel.setText(text("小节"), juce::dontSendNotification);
        beatLabel.setText(text("拍"), juce::dontSendNotification);
        bar.setComponentID("edit.spot.bar");
        beat.setComponentID("edit.spot.beat");
        bar.setInputRestrictions(7, "0123456789");
        beat.setInputRestrictions(10, "0123456789.");
        hint.setText(text("输入目标小节与拍；拍支持小数并遵循当前 Meter。Enter 置入，Esc 取消。"),
                     juce::dontSendNotification);
        hint.setFont(juce::FontOptions(12));
        status.setComponentID("edit.spot.status");
        apply.setButtonText(text("置入片段"));
        apply.setComponentID("edit.spot.apply");
        cancel.setButtonText(text("取消"));
        cancel.setComponentID("edit.spot.cancel");
        apply.onClick = [this] { applyPlacement(); };
        cancel.onClick = [this] { this->close(); };
        bar.addKeyListener(this);
        beat.addKeyListener(this);
    }

    void bind(const Json& clip, const Json& context)
    {
        clipID = clip.at("id").get<std::string>();
        binding = {{"session_token", context.at("session_token")}, {"base_revision", context.at("revision")}};
        clipName.setText(text(clip.value("name", std::string("Audio Clip"))), juce::dontSendNotification);
        bar.setText(std::to_string(clip.value("bar", 1)), false);
        beat.setText(juce::String(clip.value("beat", 1.0), 3).trimCharactersAtEnd("0").trimCharactersAtEnd("."), false);
        status.setText({}, juce::dontSendNotification);
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colours::black.withAlpha(.72f));
        graphics.setColour(base());
        graphics.fillRoundedRectangle(card.toFloat(), 8.0f);
        graphics.setColour(juce::Colour(0xff45505e));
        graphics.drawRoundedRectangle(card.toFloat(), 8.0f, 1.0f);
    }

    void resized() override
    {
        const int width = std::min(620, getWidth() - 32);
        const int height = std::min(318, getHeight() - 32);
        card = juce::Rectangle<int>(width, height).withCentre(getLocalBounds().getCentre());
        const int x = card.getX() + 24, y = card.getY() + 22, contentWidth = card.getWidth() - 48;
        title.setBounds(x, y, contentWidth, 32);
        clipName.setBounds(x, y + 43, contentWidth, 26);
        barLabel.setBounds(x, y + 92, 48, 28);
        bar.setBounds(x + 52, y + 88, 120, 34);
        beatLabel.setBounds(x + 206, y + 92, 42, 28);
        beat.setBounds(x + 252, y + 88, 120, 34);
        hint.setBounds(x, y + 142, contentWidth, 36);
        status.setBounds(x, card.getBottom() - 82, contentWidth, 24);
        apply.setBounds(card.getRight() - 238, card.getBottom() - 46, 112, 30);
        cancel.setBounds(card.getRight() - 116, card.getBottom() - 46, 92, 30);
    }

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }
        if (key == juce::KeyPress::returnKey)
        {
            applyPlacement();
            return true;
        }
        return false;
    }

    void applyPlacement()
    {
        try
        {
            const auto barText = bar.getText().trim().toStdString();
            const auto beatText = beat.getText().trim().toStdString();
            size_t parsed = 0;
            const int targetBar = std::stoi(barText, &parsed);
            if (parsed != barText.size())
                throw std::runtime_error("小节必须是正整数");
            parsed = 0;
            const double targetBeat = std::stod(beatText, &parsed);
            if (parsed != beatText.size() || targetBar < 1 || !std::isfinite(targetBeat) || targetBeat < 1)
                throw std::runtime_error("请输入有效的小节与拍");
            place(clipID, targetBar, targetBeat, binding);
        }
        catch (const std::exception& error)
        {
            status.setText(text("未执行：") + text(error.what()), juce::dontSendNotification);
        }
    }

    Place place;
    std::function<void()> close;
    std::string clipID;
    Json binding = Json::object();
    juce::Rectangle<int> card;
    juce::Label title, clipName, barLabel, beatLabel, hint, status;
    juce::TextEditor bar, beat;
    juce::TextButton apply, cancel;
};
} // namespace ndaw::desktop
