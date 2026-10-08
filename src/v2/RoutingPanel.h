#pragma once
// Included inside ndaw::desktop, after Writer and text are declared.
class RoutingPanel final : public juce::Component
{
public:
    RoutingPanel(Writer write, std::function<void()> prepare) : write(std::move(write)), prepare(std::move(prepare))
    {
        for (auto* c : std::initializer_list<juce::Component*>{&outputLabel, &output, &newAux, &reverbAux,
                                                               &newSendLabel, &target, &newPosition, &addSend,
                                                               &sendsLabel, &choice, &level, &position, &remove})
            addAndMakeVisible(c);
        outputLabel.setText(text("OUTPUT · 输出"), juce::dontSendNotification);
        newSendLabel.setText(text("新发送 · 默认 −12 dB"), juce::dontSendNotification);
        sendsLabel.setText(text("SENDS · 发送电平 / 推子位置"), juce::dontSendNotification);
        for (auto* combo : {&newPosition, &position})
        {
            combo->addItem("PRE", 1);
            combo->addItem("POST", 2);
            combo->setSelectedId(2, juce::dontSendNotification);
        }
        level.setRange(-60, 6, 0.1);
        level.setTextValueSuffix(" dB");
        level.setSliderStyle(juce::Slider::LinearHorizontal);
        level.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 24);
        output.setComponentID("routing.output");
        newAux.setComponentID("routing.new_aux");
        reverbAux.setComponentID("routing.reverb_aux");
        target.setComponentID("routing.target");
        addSend.setComponentID("send.create");
        choice.setComponentID("send.choice");
        level.setComponentID("send.level");
        position.setComponentID("send.position");
        remove.setComponentID("send.remove");
        output.onChange = [this]
        {
            int i = output.getSelectedId() - 1;
            if (i >= 0 && i < int(outputs.size()) && outputs[i] != facts["output"]["target"].get<std::string>())
                this->write("track.output", {{"track", facts["id"]}, {"target", outputs[i]}});
        };
        newAux.onClick = [this]
        {
            this->write("track.create",
                        {{"name", "Aux " + std::to_string(trackCount + 1)}, {"type", "aux"}, {"ref", "$aux"}});
        };
        reverbAux.onClick = [this] { this->prepare(); };
        target.onChange = [this]
        {
            int i = target.getSelectedId() - 1;
            if (i >= 0 && i < int(targets.size()))
                newTarget = targets[i];
        };
        addSend.onClick = [this]
        {
            if (!newTarget.empty())
                this->write("send.create", {{"track", facts["id"]},
                                            {"target", newTarget},
                                            {"db", -12},
                                            {"position", newPosition.getSelectedId() == 1 ? "pre" : "post"}});
        };
        choice.onChange = [this]
        {
            int i = choice.getSelectedId() - 1;
            if (i >= 0 && i < int(facts["sends"].size()))
                selectedSend = facts["sends"][i]["id"];
            refreshSend();
        };
        auto change = [this]
        {
            auto s = currentSend();
            if (!s.is_null() && std::abs(s["db"].get<double>() - level.getValue()) > 0.02)
                this->write("send.level", {{"send", s["id"]}, {"db", level.getValue()}});
        };
        level.onDragEnd = change;
        level.onValueChange = [this, change]
        {
            if (!level.isMouseButtonDown())
                change();
        };
        position.onChange = [this]
        {
            auto s = currentSend();
            if (!s.is_null())
            {
                std::string pos = position.getSelectedId() == 1 ? "pre" : "post";
                if (s["position"] != pos)
                    this->write("send.position", {{"send", s["id"]}, {"position", pos}});
            }
        };
        remove.onClick = [this]
        {
            auto s = currentSend();
            if (!s.is_null())
                this->write("send.remove", {{"send", s["id"]}});
        };
        target.setTextWhenNothingSelected(text("先创建 Aux"));
        choice.setTextWhenNothingSelected(text("此轨道没有发送"));
    }
    void update(const Json& selected, const Json& tracks, bool playing, const Json& midiOutputs = Json::array())
    {
        bool changed = facts.is_null() || selected.is_null() ||
                       facts.value("id", std::string{}) != selected.value("id", std::string{});
        facts = selected;
        this->playing = playing;
        trackCount = tracks.size();
        if (changed)
        {
            selectedSend.clear();
            newTarget.clear();
        }
        Json options = Json::array();
        for (const auto& t : tracks)
            options.push_back({t["id"], t["name"], t["type"]});
        Json sendOptions = Json::array();
        if (!facts.is_null())
            for (const auto& s : facts["sends"])
                sendOptions.push_back({s["id"], s["target"], s["position"]});
        auto signature =
            Json::array({midiOutputs, options, sendOptions, facts.is_null() ? Json(nullptr) : facts["id"]}).dump();
        bool rebuild = signature != lastOptions;
        if (rebuild)
        {
            lastOptions = signature;
            outputs = {"master", "none"};
            targets.clear();
            output.clear(juce::dontSendNotification);
            target.clear(juce::dontSendNotification);
            choice.clear(juce::dontSendNotification);
            output.addItem("Master / Main Out", 1);
            output.addItem("None · 仅发送", 2);
            if (!facts.is_null())
                for (const auto& t : tracks)
                    if (t["id"] != facts["id"] && t["capabilities"]["audio_routing"].get<bool>())
                    {
                        outputs.push_back(t["id"]);
                        output.addItem(text(t["name"].get<std::string>()), int(outputs.size()));
                        if (t["type"] == "aux")
                        {
                            targets.push_back(t["id"]);
                            target.addItem(text(t["name"].get<std::string>()), int(targets.size()));
                        }
                    }
        }
        if (rebuild && !facts.is_null() && (facts["type"] == "midi" || facts["type"] == "instrument"))
            for (const auto& d : midiOutputs)
                if (d["enabled"].get<bool>())
                {
                    outputs.push_back(d["id"]);
                    output.addItem(text("MIDI · ") + text(d["name"].get<std::string>()), int(outputs.size()));
                }
        if (rebuild && !facts.is_null() &&
            std::find(outputs.begin(), outputs.end(), facts["output"]["target"].get<std::string>()) == outputs.end())
        {
            outputs.push_back(facts["output"]["target"]);
            output.addItem(text("缺失输出 · ") + text(facts["output"]["target"].get<std::string>()),
                           int(outputs.size()));
        }
        if (!facts.is_null())
            for (size_t i = 0; i < outputs.size(); ++i)
                if (outputs[i] == facts["output"]["target"].get<std::string>())
                    output.setSelectedId(int(i) + 1, juce::dontSendNotification);
        if (std::find(targets.begin(), targets.end(), newTarget) == targets.end())
            newTarget = targets.empty() ? "" : targets[0];
        for (size_t i = 0; i < targets.size(); ++i)
            if (targets[i] == newTarget)
                target.setSelectedId(int(i) + 1, juce::dontSendNotification);
        if (rebuild && !facts.is_null())
            for (const auto& s : facts["sends"])
            {
                std::string name = "Bus " + std::to_string(s["bus"].get<int>() + 1);
                for (const auto& t : tracks)
                    if (t["id"] == s["target"])
                        name = t["name"];
                choice.addItem(text(name) + " · " + text(s["position"].get<std::string>()), choice.getNumItems() + 1);
            }
        if (currentSend().is_null())
            selectedSend =
                (!facts.is_null() && !facts["sends"].empty()) ? facts["sends"][0]["id"].get<std::string>() : "";
        if (!facts.is_null())
            for (size_t i = 0; i < facts["sends"].size(); ++i)
                if (facts["sends"][i]["id"] == selectedSend)
                    choice.setSelectedId(int(i) + 1, juce::dontSendNotification);
        const bool enabled = !facts.is_null() && !playing && facts["capabilities"]["audio_routing"].get<bool>();
        output.setEnabled(enabled);
        newAux.setEnabled(!playing);
        reverbAux.setEnabled(enabled);
        target.setEnabled(enabled);
        newPosition.setEnabled(enabled);
        bool duplicate = false;
        if (!facts.is_null())
            for (const auto& s : facts["sends"])
                duplicate |= s["target"] == newTarget;
        addSend.setEnabled(enabled && !newTarget.empty() && !duplicate);
        refreshSend();
    }
    void selectSend(const std::string& id)
    {
        if (facts.is_null())
            return;
        for (size_t i = 0; i < facts["sends"].size(); ++i)
            if (facts["sends"][i]["id"] == id)
            {
                selectedSend = id;
                choice.setSelectedId(int(i) + 1, juce::dontSendNotification);
                refreshSend();
                return;
            }
    }
    void resized() override
    {
        int w = getWidth() - 20;
        outputLabel.setBounds(10, 0, w, 22);
        output.setBounds(10, 26, w, 28);
        newAux.setBounds(10, 67, (w - 8) / 2, 28);
        reverbAux.setBounds(18 + (w - 8) / 2, 67, (w - 8) / 2, 28);
        newSendLabel.setBounds(10, 110, w, 22);
        target.setBounds(10, 136, w, 28);
        newPosition.setBounds(10, 174, 90, 28);
        addSend.setBounds(112, 174, w - 102, 28);
        sendsLabel.setBounds(10, 225, w, 22);
        choice.setBounds(10, 252, w, 28);
        level.setBounds(8, 294, w + 4, 30);
        position.setBounds(10, 338, 90, 28);
        remove.setBounds(112, 338, w - 102, 28);
    }

private:
    Json currentSend() const
    {
        if (!facts.is_null())
            for (const auto& s : facts["sends"])
                if (s["id"] == selectedSend)
                    return s;
        return nullptr;
    }
    void refreshSend()
    {
        auto s = currentSend();
        bool enabled = !s.is_null() && !playing;
        choice.setEnabled(enabled);
        level.setEnabled(enabled);
        position.setEnabled(enabled);
        remove.setEnabled(enabled);
        if (!s.is_null())
        {
            if (!level.isMouseButtonDown())
                level.setValue(s["db"].get<double>(), juce::dontSendNotification);
            position.setSelectedId(s["position"] == "pre" ? 1 : 2, juce::dontSendNotification);
        }
    }
    Writer write;
    std::function<void()> prepare;
    Json facts = nullptr;
    bool playing = false;
    size_t trackCount = 0;
    std::vector<std::string> outputs, targets;
    std::string selectedSend, newTarget, lastOptions;
    juce::Label outputLabel, newSendLabel, sendsLabel;
    juce::ComboBox output, target, newPosition, choice, position;
    juce::Slider level;
    juce::TextButton newAux{text("新建 Aux")}, reverbAux{text("混响 Aux…")}, addSend{text("添加发送")},
        remove{text("移除发送")};
};
