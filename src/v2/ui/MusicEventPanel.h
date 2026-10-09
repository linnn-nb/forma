#pragma once
#include "PanelTextEditor.h"
namespace ndaw::desktop
{
class MusicEventPanel final : public juce::Component
{
public:
    using Submit = std::function<std::string(Json, uint64_t, std::string)>;
    MusicEventPanel(Submit submit) : submit(std::move(submit))
    {
        setComponentID("music.event.panel");
        for (auto* c :
             std::initializer_list<juce::Component*>{&title, &events, &position, &bpm, &num, &den, &positionLabel,
                                                     &valueLabel, &status, &apply, &remove, &close})
            addAndMakeVisible(c);
        events.setComponentID("music.event.choice");
        position.setComponentID("music.event.beat");
        bpm.setComponentID("music.event.bpm");
        num.setComponentID("music.event.numerator");
        den.setComponentID("music.event.denominator");
        status.setComponentID("music.event.status");
        positionLabel.setText(text("绝对节拍位置（从 0 起）"), juce::dontSendNotification);
        position.setTooltip(text("Tracktion 拍号分拍单位；改变分母会改变该单位的时长。拍号须在小节边界。"));
        for (int d : {1, 2, 4, 8, 16, 32})
            den.addItem(juce::String(d), d);
        events.onChange = [this] { selectEvent(); };
        title.setFont(juce::FontOptions(22, juce::Font::bold));
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        for (auto* field : {&position, &bpm, &num})
            field->connect(manager);
        for (auto [button, id] :
             std::vector<std::pair<juce::TextButton*, int>>{{&apply, 275}, {&remove, 276}, {&close, 277}})
        {
            button->setComponentID("ui.command:" + juce::String(id));
            button->setCommandToTrigger(&manager, id, true);
        }
    }
    void show(const Json& facts, const std::string& kind, double beat, const std::string& event)
    {
        tempo = kind == "tempo";
        snapshot = facts;
        initialBeat = beat;
        list = facts["music"][tempo ? "tempos" : "meters"];
        events.clear(juce::dontSendNotification);
        events.addItem(text("新增事件"), 1);
        int choice = 1;
        for (size_t i = 0; i < list.size(); ++i)
        {
            const auto& v = list[i];
            const auto label =
                juce::String(v["start_beat"].get<double>(), 3) + text(" 拍 · ") +
                (tempo ? juce::String(v["bpm"].get<double>(), 2) + " BPM"
                       : juce::String(v["numerator"].get<int>()) + "/" + juce::String(v["denominator"].get<int>()));
            events.addItem(label, int(i) + 2);
            if (v["id"] == event)
                choice = int(i) + 2;
        }
        events.setSelectedId(choice, juce::dontSendNotification);
        title.setText(text(tempo ? "Tempo 事件" : "Meter 事件"), juce::dontSendNotification);
        valueLabel.setText(text(tempo ? "速度（20–300 BPM）" : "拍号（分子 / 分母）"), juce::dontSendNotification);
        bpm.setVisible(tempo);
        num.setVisible(!tempo);
        den.setVisible(!tempo);
        selectEvent();
    }
    bool canDelete() const
    {
        const int index = events.getSelectedId() - 2;
        return index >= 0 && index < int(list.size()) && list[index]["start_beat"] != 0;
    }
    void execute(bool deleting)
    {
        try
        {
            if (deleting && !canDelete())
                throw std::runtime_error("初始事件不能删除");
            const int index = events.getSelectedId() - 2;
            Json args = Json::object();
            if (index >= 0)
                args["event"] = list.at(size_t(index))["id"];
            if (!deleting)
            {
                args["beat_position"] = number(position);
                if (tempo)
                    args["bpm"] = number(bpm);
                else
                {
                    const double value = number(num);
                    if (value != std::floor(value))
                        throw std::runtime_error("拍号分子须为整数");
                    args["numerator"] = int(value);
                    args["denominator"] = den.getSelectedId();
                }
            }
            const std::string command = std::string(tempo ? "tempo" : "meter") + ".event." +
                                        (deleting    ? "delete"
                                         : index < 0 ? "create"
                                                     : "set");
            const auto error =
                submit(Json::array({operation(command, args)}), snapshot["revision"], snapshot["session_token"]);
            if (!error.empty())
                throw std::runtime_error(error);
        }
        catch (const std::exception& e)
        {
            status.setText(text("未提交：") + text(e.what()), juce::dontSendNotification);
        }
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1b232b));
        g.setColour(accent());
        g.drawRect(getLocalBounds().reduced(18), 1);
    }
    void resized() override
    {
        const int x = std::max(36, (getWidth() - 650) / 2), y = std::max(40, (getHeight() - 350) / 2);
        title.setBounds(x, y, 650, 36);
        events.setBounds(x, y + 52, 624, 30);
        positionLabel.setBounds(x, y + 101, 260, 28);
        position.setBounds(x + 274, y + 101, 350, 28);
        valueLabel.setBounds(x, y + 145, 260, 28);
        bpm.setBounds(x + 274, y + 145, 350, 28);
        num.setBounds(x + 274, y + 145, 168, 28);
        den.setBounds(x + 458, y + 145, 166, 28);
        status.setBounds(x, y + 194, 624, 80);
        apply.setBounds(x + 250, y + 292, 116, 32);
        remove.setBounds(x + 378, y + 292, 116, 32);
        close.setBounds(x + 506, y + 292, 118, 32);
    }

private:
    static double number(const juce::TextEditor& editor)
    {
        const auto s = editor.getText().toStdString();
        size_t used = 0;
        const double n = std::stod(s, &used);
        if (used != s.size() || !std::isfinite(n) || std::abs(n) > 1e8)
            throw std::runtime_error("请输入完整有限数值");
        return n;
    }
    void selectEvent()
    {
        const int index = events.getSelectedId() - 2;
        const auto value = index < 0 ? snapshot["music"] : list.at(size_t(index));
        position.setText(juce::String(index < 0 ? initialBeat : value["start_beat"].get<double>(), 10), false);
        bpm.setText(juce::String(value.value("bpm", snapshot["music"]["bpm"].get<double>()), 6), false);
        num.setText(juce::String(value.value("numerator", snapshot["music"]["numerator"].get<int>())), false);
        den.setSelectedId(value.value("denominator", snapshot["music"]["denominator"].get<int>()),
                          juce::dontSendNotification);
        position.setEnabled(index < 0 || value["start_beat"] != 0);
        remove.setEnabled(canDelete());
        auto note = text("保存为一笔人工事务。初始事件保留；冲突时不会覆盖后来的编辑。");
        const auto repairs = snapshot["music"].value("id_repairs", Json::array());
        if (!repairs.empty())
            note += text(" 本工程载入时修复了 ") + juce::String(int(repairs.size())) + text(" 个重复事件 ID；首项 ") +
                    text(repairs[0]["previous_id"].get<std::string>()) + " → " +
                    text(repairs[0]["replacement_id"].get<std::string>());
        status.setText(note, juce::dontSendNotification);
    }
    Submit submit;
    juce::Label title, positionLabel, valueLabel, status;
    juce::ComboBox events, den;
    PanelTextEditor position, bpm, num;
    juce::TextButton apply{text("提交")}, remove{text("删除事件")}, close{text("取消")};
    Json snapshot, list;
    bool tempo = true;
    double initialBeat = 0;
};
} // namespace ndaw::desktop
