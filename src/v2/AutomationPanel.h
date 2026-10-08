#pragma once
// Included inside ndaw::desktop. This view receives only queried facts and L1 callbacks.
class AutomationPanel final : public juce::Component
{
public:
    using EditWriter = std::function<void(const std::string&, Json, uint64_t)>;
    using Sampler = std::function<Json(const std::string&, const std::string&, int64_t)>;
    AutomationPanel(EditWriter edit, Writer control, Sampler sample)
        : edit(std::move(edit)), control(std::move(control)), sample(std::move(sample)), canvas(*this)
    {
        for (auto* c : std::initializer_list<juce::Component*>{
                 &modeTitle, &mode, &laneTitle, &lane, &value, &canvas, &point, &position, &pointValue, &curve,
                 &positionTitle, &valueTitle, &curveTitle, &add, &apply, &remove, &clear, &explanation})
            addAndMakeVisible(c);
        modeTitle.setText(text("MODE · 轨道自动化"), juce::dontSendNotification);
        laneTitle.setText(text("LANE · 实际参数"), juce::dontSendNotification);
        for (auto name : {"Read", "Touch", "Latch", "Write"})
            mode.addItem(name, mode.getNumItems() + 1);
        mode.setComponentID("automation.mode");
        lane.setComponentID("automation.lane");
        value.setComponentID("automation.value");
        point.setComponentID("automation.point");
        position.setComponentID("automation.position");
        pointValue.setComponentID("automation.point_value");
        curve.setComponentID("automation.curve");
        add.setComponentID("automation.point.add");
        apply.setComponentID("automation.point.set");
        remove.setComponentID("automation.point.delete");
        clear.setComponentID("automation.clear");
        canvas.setComponentID("automation.canvas");
        mode.onChange = [this]
        {
            if (!facts.is_null() && mode.getText().toLowerCase().toStdString() != facts["mode"].get<std::string>())
                this->edit("automation.mode",
                           {{"track", facts["track"]}, {"mode", mode.getText().toLowerCase().toStdString()}}, revision);
        };
        lane.onChange = [this]
        {
            selectedLane = lane.getSelectedId() - 1;
            selectedPoint.clear();
            lastCurve.clear();
            refreshLane(true);
        };
        point.onChange = [this]
        {
            int i = point.getSelectedId() - 1;
            auto p = points();
            if (i >= 0 && i < int(p.size()))
            {
                selectedPoint = p[i]["id"];
                fillPoint(p[i]);
                canvas.repaint();
            }
        };
        value.setSliderStyle(juce::Slider::LinearHorizontal);
        value.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
        value.onDragStart = [this]
        {
            if (live())
            {
                gesture = true;
                sendControl("begin");
            }
        };
        value.onValueChange = [this]
        {
            if (gesture)
                sendControl("value");
            else if (live() && !value.isMouseButtonDown())
            {
                sendControl("begin");
                sendControl("value");
                sendControl("end");
            }
        };
        value.onDragEnd = [this]
        {
            if (gesture)
            {
                sendControl("end");
                gesture = false;
            }
        };
        positionTitle.setText(text("位置 · samples"), juce::dontSendNotification);
        valueTitle.setText(text("实际参数值"), juce::dontSendNotification);
        curveTitle.setText(text("曲线 −1…1"), juce::dontSendNotification);
        positionTitle.setFont(juce::FontOptions(11));
        valueTitle.setFont(juce::FontOptions(11));
        curveTitle.setFont(juce::FontOptions(10));
        position.setInputRestrictions(16, "0123456789");
        position.setTooltip(text("工程采样位置，48 kHz；整数，不是源媒体位置"));
        pointValue.setTooltip(text("实际参数单位与范围；音量 / VCA 使用 dB"));
        curve.setTooltip(text("Tracktion 曲线形状 −1…1；1 为阶跃"));
        add.onClick = [this]
        {
            auto l = currentLane();
            if (l.is_null())
                return;
            this->edit("automation.point.add",
                       {{"track", facts["track"]},
                        {"parameter", l["id"]},
                        {"ref", "$point"},
                        {"position_samples", playhead},
                        {"value", l["value"]},
                        {"curve", 0}},
                       revision);
        };
        apply.onClick = [this]
        {
            if (selectedPoint.empty())
                return;
            auto pos = number(position), v = number(pointValue), shape = number(curve);
            if (!pos || !v || !shape)
                return;
            this->edit("automation.point.set",
                       {{"track", facts["track"]},
                        {"parameter", currentLane()["id"]},
                        {"point", selectedPoint},
                        {"position_samples", int64_t(*pos)},
                        {"value", *v},
                        {"curve", *shape}},
                       revision);
        };
        remove.onClick = [this]
        {
            if (!selectedPoint.empty())
                this->edit("automation.point.delete",
                           {{"track", facts["track"]}, {"parameter", currentLane()["id"]}, {"point", selectedPoint}},
                           revision);
        };
        clear.onClick = [this]
        {
            auto l = currentLane();
            if (!l.is_null())
                this->edit("automation.clear", {{"track", facts["track"]}, {"parameter", l["id"]}}, revision);
        };
        explanation.setFont(juce::FontOptions(12));
        setSize(302, 650);
    }
    void update(Json query, bool playing, int64_t playhead, int64_t length)
    {
        auto track = query.is_null() ? "" : query["track"].get<std::string>();
        bool changed = track != target;
        target = track;
        facts = std::move(query);
        this->playing = playing;
        this->playhead = playhead;
        end = std::max(int64_t(480000), length);
        revision = facts.is_null() ? 0 : facts["revision"].get<uint64_t>();
        Json ids = Json::array();
        if (!facts.is_null())
            for (const auto& l : facts["lanes"])
                ids.push_back({l["id"], l["name"]});
        if (changed || ids.dump() != lastLanes)
        {
            lastLanes = ids.dump();
            selectedLane = changed ? 0 : std::clamp(selectedLane, 0, std::max(0, int(ids.size()) - 1));
            if (changed && !facts.is_null())
                for (int i = 0; i < int(facts["lanes"].size()); ++i)
                    if (facts["lanes"][i]["parameter"] == "volume" || facts["lanes"][i]["parameter"] == "vca")
                    {
                        selectedLane = i;
                        break;
                    }
            selectedPoint.clear();
            lane.clear(juce::dontSendNotification);
            int i = 1;
            if (!facts.is_null())
                for (const auto& l : facts["lanes"])
                    lane.addItem(text(l["name"].get<std::string>()), i++);
            lane.setSelectedId(ids.empty() ? 0 : selectedLane + 1, juce::dontSendNotification);
            lastCurve.clear();
        }
        const bool supported = !ids.empty();
        mode.setEnabled(supported && !playing);
        lane.setEnabled(supported && !gesture);
        if (supported)
        {
            int i = 1;
            for (auto name : {"read", "touch", "latch", "write"})
            {
                if (facts["mode"] == name)
                    mode.setSelectedId(i, juce::dontSendNotification);
                ++i;
            }
        }
        if (!playing)
            gesture = false;
        refreshLane(changed);
        explanation.setText(
            !supported ? text("该轨道没有可自动化参数。")
            : !facts["capture"].is_null()
                ? text("实际录写中 · 停止后整段成为一笔 human 事务，可 Undo。\nTouch 松手返回；Latch / Write "
                       "保持至停止。")
                : text("停止时编辑采样位置与参数值。\n双击曲线加点，拖动点移动，右键删除。\nTouch / Latch / Write "
                       "播放后拖动参数录写。\nWrite 只自动录写已有曲线，停止后转 Latch。"),
            juce::dontSendNotification);
    }
    void resized() override
    {
        int w = getWidth() - 20;
        modeTitle.setBounds(10, 0, w, 22);
        mode.setBounds(10, 26, w, 28);
        laneTitle.setBounds(10, 64, w, 22);
        lane.setBounds(10, 90, w, 28);
        value.setBounds(10, 128, w, 30);
        canvas.setBounds(10, 170, w, 160);
        point.setBounds(10, 341, w, 28);
        positionTitle.setBounds(10, 371, w / 2 - 4, 18);
        valueTitle.setBounds(14 + w / 2, 371, w / 2 - 4, 18);
        position.setBounds(10, 391, w / 2 - 4, 28);
        pointValue.setBounds(14 + w / 2, 391, w / 2 - 4, 28);
        curveTitle.setBounds(10, 421, 80, 16);
        curve.setBounds(10, 440, 80, 28);
        apply.setBounds(98, 440, w - 88, 28);
        add.setBounds(10, 480, w, 28);
        remove.setBounds(10, 519, w / 2 - 4, 28);
        clear.setBounds(14 + w / 2, 519, w / 2 - 4, 28);
        explanation.setBounds(10, 556, w, 94);
    }

private:
    std::optional<double> number(juce::TextEditor& field)
    {
        try
        {
            auto s = field.getText().trim().toStdString();
            size_t count = 0;
            double v = std::stod(s, &count);
            if (count != s.size() || !std::isfinite(v) ||
                (&field == &position && (v < 0 || v > double(int64_t(1) << 53) || v != std::floor(v))))
                throw std::runtime_error("invalid");
            return v;
        }
        catch (...)
        {
            explanation.setText(text("输入无效：位置必须是非负整数；值和曲线必须是有限数值。工程未修改。"),
                                juce::dontSendNotification);
            return {};
        }
    }
    Json currentLane() const
    {
        return !facts.is_null() && selectedLane >= 0 && selectedLane < int(facts["lanes"].size())
                   ? facts["lanes"][selectedLane]
                   : Json(nullptr);
    }
    Json points() const
    {
        auto l = currentLane();
        return l.is_null() ? Json::array() : l["points"];
    }
    bool live() const
    {
        return playing && !facts.is_null() && !facts["capture"].is_null() && facts["mode"] != "read" &&
               !currentLane().is_null();
    }
    void sendControl(const char* action)
    {
        Json args = {{"track", facts["track"]}, {"parameter", currentLane()["id"]}};
        if (std::string(action) == "value")
            args["value"] = value.getValue();
        control(std::string("automation.gesture.") + action, args);
    }
    void fillPoint(const Json& p)
    {
        position.setText(juce::String(p["position_samples"].get<int64_t>()), false);
        pointValue.setText(juce::String(p["value"].get<double>(), 5), false);
        curve.setText(juce::String(p["curve"].get<double>(), 3), false);
    }
    void refreshLane(bool force)
    {
        auto l = currentLane();
        bool valid = !l.is_null();
        value.setEnabled(live());
        bool editable = valid && !playing;
        for (auto* c : std::initializer_list<juce::Component*>{&add, &clear, &position, &pointValue, &curve})
            c->setEnabled(editable);
        apply.setEnabled(editable && !selectedPoint.empty());
        remove.setEnabled(editable && !selectedPoint.empty());
        point.setEnabled(valid && !playing);
        if (valid)
        {
            value.setRange(l["minimum"], l["maximum"], 0);
            value.setTextValueSuffix(" " + text(l["unit"].get<std::string>()));
            if (!gesture && !value.isMouseButtonDown())
                value.setValue(l["value"].get<double>(), juce::dontSendNotification);
        }
        std::string signature = valid ? Json::array({l["id"], l["points"], end, l["explicit_value"]}).dump() : "";
        if (force || signature != lastCurve)
        {
            lastCurve = signature;
            point.clear(juce::dontSendNotification);
            int i = 1;
            bool found = false;
            for (const auto& p : points())
            {
                point.addItem(juce::String(p["position_samples"].get<int64_t>()) + " · " +
                                  juce::String(p["value"].get<double>(), 2),
                              i);
                if (p["id"] == selectedPoint)
                {
                    point.setSelectedId(i, juce::dontSendNotification);
                    found = true;
                    fillPoint(p);
                }
                ++i;
            }
            if (!found)
            {
                selectedPoint.clear();
                if (!points().empty())
                {
                    auto p = points()[0];
                    selectedPoint = p["id"];
                    point.setSelectedId(1, juce::dontSendNotification);
                    fillPoint(p);
                }
            }
            samples = valid ? sample(target, l["id"], end) : Json::array();
        }
        apply.setEnabled(editable && !selectedPoint.empty());
        remove.setEnabled(editable && !selectedPoint.empty());
        canvas.repaint();
    }
    class Canvas final : public juce::Component
    {
    public:
        explicit Canvas(AutomationPanel& owner) : p(owner) {}
        juce::Point<float> xy(int64_t pos, double value) const
        {
            auto l = p.currentLane();
            double lo = l["minimum"], hi = l["maximum"];
            return {8 + float(pos / double(p.end)) * (getWidth() - 16),
                    8 + float(1 - (value - lo) / (hi - lo)) * (getHeight() - 30)};
        }
        Json at(juce::Point<float> pos) const
        {
            auto l = p.currentLane();
            return {
                {"position_samples", std::llround(std::clamp((pos.x - 8) / double(getWidth() - 16), 0., 1.) * p.end)},
                {"value", l["maximum"].get<double>() - std::clamp((pos.y - 8) / double(getHeight() - 30), 0., 1.) *
                                                           (l["maximum"].get<double>() - l["minimum"].get<double>())}};
        }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff141e28));
            auto l = p.currentLane();
            if (l.is_null())
                return;
            g.setColour(juce::Colour(0xff334452));
            for (int i = 1; i < 4; ++i)
                g.drawHorizontalLine(8 + i * (getHeight() - 30) / 4, 8, float(getWidth() - 8));
            juce::Path path;
            bool start = true;
            for (const auto& s : p.samples)
            {
                auto point = xy(s["position_samples"], s["value"]);
                if (start)
                {
                    path.startNewSubPath(point);
                    start = false;
                }
                else
                    path.lineTo(point);
            }
            g.setColour(accent());
            g.strokePath(path, juce::PathStrokeType(1.5f));
            for (const auto& pt : p.points())
            {
                auto point = xy(pt["position_samples"], pt["value"]);
                g.setColour(pt["id"] == p.selectedPoint ? juce::Colour(0xffedca72) : accent());
                g.fillEllipse(point.x - 3, point.y - 3, 6, 6);
            }
            if (!drag.is_null())
            {
                auto point = xy(drag["position_samples"], drag["value"]);
                g.setColour(juce::Colours::white);
                g.drawEllipse(point.x - 5, point.y - 5, 10, 10, 1.5f);
            }
            g.setColour(juce::Colour(0xffedca72));
            int x = 8 + int(p.playhead / double(p.end) * (getWidth() - 16));
            g.drawVerticalLine(x, 8, float(getHeight() - 22));
            g.setColour(juce::Colour(0xffa8beca));
            g.setFont(juce::FontOptions(10));
            g.drawText("0                       " + juce::String(p.end / 48000., 1) + " s", 8, getHeight() - 20,
                       getWidth() - 16, 18, juce::Justification::centred);
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (p.playing || p.currentLane().is_null())
                return;
            drag = nullptr;
            nearest.clear();
            float best = 10;
            for (const auto& pt : p.points())
            {
                float d = xy(pt["position_samples"], pt["value"]).getDistanceFrom(e.position);
                if (d < best)
                {
                    best = d;
                    nearest = pt["id"];
                    original = pt;
                }
            }
            if (!nearest.empty())
            {
                p.selectedPoint = nearest;
                for (int i = 0; i < int(p.points().size()); ++i)
                    if (p.points()[i]["id"] == nearest)
                        p.point.setSelectedId(i + 1, juce::sendNotificationSync);
                revision = p.revision;
                lane = p.currentLane()["id"];
                track = p.target;
                if (e.mods.isPopupMenu())
                {
                    p.edit("automation.point.delete", {{"track", track}, {"parameter", lane}, {"point", nearest}},
                           revision);
                    nearest.clear();
                }
            }
            repaint();
        }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (!nearest.empty() && !p.playing)
            {
                drag = at(e.position);
                repaint();
            }
        }
        void mouseUp(const juce::MouseEvent&) override
        {
            if (!drag.is_null() && !p.playing)
            {
                auto args = drag;
                args.update({{"track", track}, {"parameter", lane}, {"point", nearest}, {"curve", original["curve"]}});
                drag = nullptr;
                p.edit("automation.point.set", args, revision);
            }
            nearest.clear();
        }
        void mouseDoubleClick(const juce::MouseEvent& e) override
        {
            if (!p.playing && !p.currentLane().is_null() && nearest.empty())
            {
                auto args = at(e.position);
                args.update(
                    {{"track", p.target}, {"parameter", p.currentLane()["id"]}, {"ref", "$point"}, {"curve", 0}});
                p.edit("automation.point.add", args, p.revision);
            }
        }

    private:
        AutomationPanel& p;
        Json drag = nullptr, original;
        std::string nearest, lane, track;
        uint64_t revision = 0;
    };
    EditWriter edit;
    Writer control;
    Sampler sample;
    Json facts = nullptr, samples = Json::array();
    std::string target, lastLanes, lastCurve, selectedPoint;
    int selectedLane = 0;
    uint64_t revision = 0;
    bool playing = false, gesture = false;
    int64_t playhead = 0, end = 480000;
    juce::Label modeTitle, laneTitle, explanation, positionTitle, valueTitle, curveTitle;
    juce::ComboBox mode, lane, point;
    juce::Slider value;
    Canvas canvas;
    juce::TextEditor position, pointValue, curve;
    juce::TextButton add{text("在播放头加点")}, apply{text("应用采样位置 / 值 / 曲线")}, remove{text("删选中点")},
        clear{text("清空曲线")};
};
