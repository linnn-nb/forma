#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class TrackHeader final : public juce::Component
{
public:
    TrackHeader(std::string id, bool strip, Writer write, std::function<void(std::string)> select)
        : id(std::move(id)), strip(strip), write(std::move(write)), select(std::move(select))
    {
        for (auto* b : {&name, &mute, &solo, &safe, &fold})
            addAndMakeVisible(b);
        addAndMakeVisible(gain);
        name.setComponentID("track.select:" + text(this->id));
        name.onClick = [this] { this->select(this->id); };
        mute.setComponentID("track.mute:" + text(this->id));
        solo.setComponentID("track.solo:" + text(this->id));
        safe.setComponentID("track.solo_safe:" + text(this->id));
        mute.setTooltip(text("静音 · Mute"));
        solo.setTooltip(text("独听 · Solo"));
        safe.setTooltip(text("Solo Safe · 其他轨道独听时仍可听；显式静音优先"));
        for (auto* b : {&mute, &solo, &safe})
            b->setClickingTogglesState(true);
        mute.onClick = [this]
        { this->write("track.mute", {{"track", this->id}, {"enabled", !facts.value("mute", false)}}); };
        solo.onClick = [this]
        { this->write("track.solo", {{"track", this->id}, {"enabled", !facts.value("solo", false)}}); };
        safe.onClick = [this]
        { this->write("track.solo_safe", {{"track", this->id}, {"enabled", !facts.value("solo_safe", false)}}); };
        fold.setComponentID("track.fold:" + text(this->id));
        fold.onClick = [this]
        { this->write("track.collapsed", {{"track", this->id}, {"enabled", !facts.value("collapsed", false)}}); };
        gain.setComponentID("track.gain:" + text(this->id));
        gain.setRange(-60, 6, 0.1);
        gain.setTextValueSuffix(" dB");
        gain.setSliderStyle(strip ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
        gain.setTextBoxStyle(strip ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight, false, 72, 24);
        gain.onDragStart = [this]
        {
            stoppedGesture = false;
            if (live())
            {
                gesture = true;
                control("begin");
            }
        };
        gain.onDragEnd = [this]
        {
            if (stoppedGesture)
            {
                stoppedGesture = false;
                return;
            }
            if (gesture)
            {
                control("end");
                gesture = false;
            }
            else
                changeGain();
        };
        gain.onValueChange = [this]
        {
            if (gesture)
                control("value");
            else if (!gain.isMouseButtonDown())
            {
                if (live())
                {
                    control("begin");
                    control("value");
                    control("end");
                }
                else
                    changeGain();
            }
        };
        addAndMakeVisible(pan);
        addAndMakeVisible(panLaw);
        pan.setComponentID("track.pan:" + text(this->id));
        pan.setRange(-1, 1, .01);
        pan.setDoubleClickReturnValue(true, 0);
        pan.setSliderStyle(strip ? juce::Slider::LinearHorizontal : juce::Slider::RotaryHorizontalVerticalDrag);
        pan.setTextBoxStyle(strip ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow, false, strip ? 58 : 76,
                            20);
        pan.textFromValueFunction = [](double p)
        {
            return std::abs(p) <= .005
                       ? juce::String("C")
                       : juce::String(p < 0 ? "L " : "R ") + juce::String(std::round(std::abs(p) * 100), 0);
        };
        pan.valueFromTextFunction = [](const juce::String& value)
        {
            auto v = value.trim().toUpperCase();
            if (v == "C")
                return 0.;
            if (v.startsWith("L"))
                return -v.substring(1).getDoubleValue() / 100.;
            if (v.startsWith("R"))
                return v.substring(1).getDoubleValue() / 100.;
            return v.getDoubleValue() / 100.;
        };
        pan.setTooltip(
            text("Pan / Balance · L100–C–R100；双击居中。音频声道增益，不交换立体声声道，不发送 MIDI CC10。"));
        pan.onDragStart = [this]
        {
            panStoppedGesture = false;
            if (live())
            {
                panGesture = true;
                panControl("begin");
            }
        };
        pan.onDragEnd = [this]
        {
            if (panStoppedGesture)
            {
                panStoppedGesture = false;
                return;
            }
            if (panGesture)
            {
                panControl("end");
                panGesture = false;
            }
            else
                changePan();
        };
        pan.onValueChange = [this]
        {
            if (panGesture)
                panControl("value");
            else if (!pan.isMouseButtonDown())
            {
                if (live())
                {
                    panControl("begin");
                    panControl("value");
                    panControl("end");
                }
                else
                    changePan();
            }
        };
        panLaw.setComponentID("track.pan_law:" + text(this->id));
        int lawID = 1;
        for (const auto& law : Commands::panLawCatalog())
            panLaw.addItem(text(law["label"]), lawID++);
        panLaw.setTooltip(
            text("Pan Law · 停止时切换。Linear 两端保留声道 +6.02 dB；其他曲线两端为原增益，中心按曲线衰减。"));
        panLaw.onChange = [this]
        {
            const int i = panLaw.getSelectedId() - 1;
            const auto laws = Commands::panLawCatalog();
            if (i >= 0 && i < int(laws.size()) && laws[i]["id"] != facts.value("pan_law_setting", Json(nullptr)))
                this->write("track.pan_law", {{"track", this->id}, {"law", laws[i]["id"]}});
        };
    }
    void update(const Json& value, bool selected)
    {
        facts = value;
        this->selected = selected;
        name.setButtonText(text(facts["name"].get<std::string>()));
        name.setColour(juce::TextButton::buttonColourId, trackColour(facts).darker(selected ? .45f : .75f));
        mute.setToggleState(facts["mute"], juce::dontSendNotification);
        solo.setToggleState(facts["solo"], juce::dontSendNotification);
        safe.setToggleState(facts["solo_safe"], juce::dontSendNotification);
        mute.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff965249));
        solo.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffa38236));
        bool hasGain = facts["capabilities"]["gain"];
        gain.setVisible(hasGain);
        gain.setEnabled(
            hasGain && !facts.value("recording", false) && (!facts.value("automation_writing", false) || live()) &&
            !(facts.value("playing", false) && facts.value("automation_mode", std::string("read")) == "read" &&
              facts.value("automation_volume_points", 0) > 0));
        if (hasGain && !gesture && !gain.isMouseButtonDown())
            gain.setValue(facts["gain_db"].get<double>(), juce::dontSendNotification);
        if (!facts.value("playing", false) && gesture)
        {
            gesture = false;
            stoppedGesture = true;
        }
        for (auto* b : {&mute, &solo, &safe})
            b->setEnabled(!facts.value("automation_writing", false) && !facts.value("recording", false));
        bool hasPan = facts["capabilities"].value("pan", false);
        pan.setVisible(hasPan);
        panLaw.setVisible(strip && hasPan);
        pan.setEnabled(
            hasPan && !facts.value("recording", false) && (!facts.value("automation_writing", false) || live()) &&
            !(facts.value("playing", false) && facts.value("automation_mode", std::string("read")) == "read" &&
              facts.value("automation_pan_points", 0) > 0));
        if (hasPan && !panGesture && !pan.isMouseButtonDown())
            pan.setValue(facts["pan"].get<double>(), juce::dontSendNotification);
        if (!facts.value("playing", false) && panGesture)
        {
            panGesture = false;
            panStoppedGesture = true;
        }
        panLaw.setEnabled(hasPan && !facts.value("playing", false) && !facts.value("recording", false) &&
                          !facts.value("automation_writing", false));
        if (hasPan)
        {
            int i = 1, selectedLaw = 0;
            for (const auto& law : Commands::panLawCatalog())
            {
                if (law["id"] == facts["pan_law_setting"])
                    selectedLaw = i;
                ++i;
            }
            panLaw.setSelectedId(selectedLaw, juce::dontSendNotification);
        }
        fold.setVisible(facts["capabilities"]["group"]);
        fold.setButtonText(facts["collapsed"].get<bool>() ? ">" : "v");
        fold.setEnabled(!facts.value("playing", false));
        resized();
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(strip ? 0xff222b35 : 0xff252e39));
        g.setColour(trackColour(facts));
        g.fillRect(0, 0, 4, getHeight());
        g.setColour(juce::Colour(0xff8394a7));
        g.setFont(juce::FontOptions(11));
        if (strip)
        {
            g.drawText(text("INSERTS · 插入"), 14, 87, getWidth() - 28, 22, juce::Justification::left);
            int y = 115;
            for (const auto& p : facts.value("plugins", Json::array()))
            {
                g.setColour(juce::Colour(0xff17202a));
                g.fillRoundedRectangle(juce::Rectangle<float>(14, float(y), float(getWidth() - 28), 24), 3);
                g.setColour(p["bypassed"].get<bool>() ? juce::Colour(0xff738396) : juce::Colour(0xffbce5de));
                g.drawText(text(p["name"].get<std::string>()), 20, y, getWidth() - 40, 24, juce::Justification::left);
                y += 28;
            }
            g.setColour(juce::Colour(0xffa8beca));
            g.drawText(text("OUT · ") + text(facts["output"]["name"].get<std::string>()), 14, 257, getWidth() - 28, 22,
                       juce::Justification::left);
            g.drawText(text("SENDS · ") + juce::String(facts["sends"].size()), 14, 229, getWidth() - 28, 22,
                       juce::Justification::left);
            if (facts.value("plugins", Json::array()).empty())
                g.drawText(text("无效果器"), 14, 115, getWidth() - 28, 24, juce::Justification::left);
            if (pan.isVisible())
                g.drawText(text("PAN / BALANCE"), 14, 288, getWidth() - 28, 19, juce::Justification::left);
            g.setColour(juce::Colour(0xff8394a7));
            g.drawText(text(facts.value("type", std::string("audio"))) +
                           text(facts.value("audible", false) ? " · 可听" : " · 已静音 / 非独听"),
                       14, getHeight() - 30, getWidth() - 28, 22, juce::Justification::centred);
        }
        else
            g.drawText(text(facts.value("type", std::string("audio"))) + "  |  " +
                           juce::String(facts.value("clips", Json::array()).size()) + text(" 片段"),
                       12, 116, pan.isVisible() ? 128 : getWidth() - 24, 19, juce::Justification::left);
    }
    void resized() override
    {
        int inset = strip ? 12 : 12 + std::min(60, (facts.is_object() ? facts.value("depth", 0) : 0) * 12);
        bool group = !facts.is_null() && facts.contains("capabilities") && facts["capabilities"]["group"].get<bool>();
        fold.setBounds(inset, 10, 22, 28);
        name.setBounds(inset + (group ? 26 : 0), 10, getWidth() - inset - 12 - (group ? 26 : 0), 28);
        int width = (getWidth() - 36) / 3;
        mute.setBounds(12, 46, width, 25);
        solo.setBounds(18 + width, 46, width, 25);
        safe.setBounds(24 + 2 * width, 46, width, 25);
        const bool hasPan = pan.isVisible();
        const int faderTop = strip && hasPan ? 378 : 292;
        gain.setBounds(strip ? 22 : 10, strip ? faderTop : 82,
                       strip    ? getWidth() - 44
                       : hasPan ? getWidth() - 112
                                : getWidth() - 20,
                       strip ? std::max(60, getHeight() - faderTop - 44) : 29);
        pan.setBounds(strip ? 12 : getWidth() - 96, strip ? 310 : 76, strip ? getWidth() - 24 : 84, strip ? 30 : 60);
        panLaw.setBounds(12, 346, getWidth() - 24, 25);
    }

private:
    bool live() const
    {
        return facts.value("playing", false) && facts.value("automation_writing", false) &&
               facts.value("automation_mode", std::string("read")) != "read";
    }
    void control(const char* action)
    {
        Json args = {{"track", id}, {"parameter", facts.value("type", std::string{}) == "vca" ? "vca" : "volume"}};
        if (std::string(action) == "value")
            args["value"] = gain.getValue();
        write(std::string("automation.gesture.") + action, args);
    }
    void changeGain()
    {
        if (std::abs(gain.getValue() - facts.value("gain_db", 0.)) > 0.001)
            write("track.gain", {{"track", id}, {"db", gain.getValue()}});
    }
    void panControl(const char* action)
    {
        Json args = {{"track", id}, {"parameter", "pan"}};
        if (std::string(action) == "value")
            args["value"] = pan.getValue();
        write(std::string("automation.gesture.") + action, args);
    }
    void changePan()
    {
        if (std::abs(pan.getValue() - facts.value("pan", 0.)) > .0001)
            write("track.pan", {{"track", id}, {"value", pan.getValue()}});
    }
    std::string id;
    bool strip, selected = false, gesture = false, stoppedGesture = false, panGesture = false,
                panStoppedGesture = false;
    Writer write;
    std::function<void(std::string)> select;
    Json facts;
    juce::TextButton name, mute{"M"}, solo{"S"}, safe{"SAFE"}, fold{"v"};
    juce::Slider gain, pan;
    juce::ComboBox panLaw;
};

} // namespace ndaw::desktop
