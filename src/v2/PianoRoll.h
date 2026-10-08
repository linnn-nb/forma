#pragma once
// Included by Workspace.h inside ndaw::desktop. This view only reads facts and emits L1 commands.
using MusicalWriter = std::function<void(const std::string&, Json, uint64_t)>;
class NoteCanvas final : public juce::Component
{
public:
    NoteCanvas(MusicalWriter writer, std::function<int64_t(double)> sample,
               std::function<Json(int64_t, int64_t, double)> grid, std::function<void()> selection)
        : write(std::move(writer)), sample(std::move(sample)), grid(std::move(grid)), selection(std::move(selection))
    {
        setWantsKeyboardFocus(true);
        setComponentID("midi.canvas");
    }
    void update(const Json& value, uint64_t revision, bool playing, double snap, int width, double positionBeat)
    {
        if (clip.is_null() != value.is_null() || (!value.is_null() && (clip.is_null() || clip["id"] != value["id"])))
        {
            selected.clear();
            selectedIDs.clear();
        }
        clip = value;
        this->revision = revision;
        this->playing = playing;
        this->snap = snap;
        this->positionBeat = positionBeat;
        std::set<std::string> valid;
        if (!clip.is_null())
            for (const auto& n : clip["notes"])
                if (selectedIDs.contains(n["id"].get<std::string>()))
                    valid.insert(n["id"]);
        selectedIDs = std::move(valid);
        if (selectedNote().is_null())
            selected = selectedIDs.empty() ? "" : *selectedIDs.begin();
        setSize(std::max(width, clip.is_null() ? width : 64 + int(clip["length_beats"].get<double>() * 72)),
                32 + 128 * 14);
        repaint();
    }
    Json selectedNote() const
    {
        if (!clip.is_null())
            for (const auto& n : clip["notes"])
                if (n["id"] == selected)
                    return n;
        return nullptr;
    }
    Json selectedNotes() const
    {
        Json result = Json::array();
        if (!clip.is_null())
            for (const auto& n : clip["notes"])
                if (selectedIDs.contains(n["id"].get<std::string>()))
                    result.push_back(n["id"]);
        return result;
    }
    void selectAll()
    {
        if (playing || clip.is_null())
            return;
        selectedIDs.clear();
        for (const auto& n : clip["notes"])
            selectedIDs.insert(n["id"]);
        selected = selectedIDs.empty() ? "" : *selectedIDs.begin();
        selection();
        repaint();
    }
    Json selectedArguments() const
    {
        auto n = selectedNote();
        if (n.is_null())
            return nullptr;
        return {{"clip", clip["id"]},
                {"note", n["id"]},
                {"pitch", n["pitch"]},
                {"velocity", n["velocity"]},
                {"position_samples", n["position_samples"]},
                {"length_samples", n["length_samples"]}};
    }
    void setVelocity(int velocity)
    {
        auto a = selectedArguments();
        if (!a.is_null() && !playing && a["velocity"] != velocity)
        {
            a["velocity"] = velocity;
            write("midi.note.set", a, revision);
        }
    }
    void removeSelected()
    {
        auto n = selectedNote();
        if (!n.is_null() && !playing)
            write("midi.note.delete", {{"clip", clip["id"]}, {"note", n["id"]}}, revision);
    }
    juce::Rectangle<float> noteBounds(const Json& note) const
    {
        const auto start = note["start_beat"].get<double>() - clip["start_beat"].get<double>();
        return {float(64 + start * scale()), float(32 + (127 - note["pitch"].get<int>()) * 14 + 1),
                float(std::max(3., note["length_beats"].get<double>() * scale())), 12};
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
        g.setFont(juce::FontOptions(10));
        for (int pitch = 127; pitch >= 0; --pitch)
        {
            int y = 32 + (127 - pitch) * 14;
            const int pc = pitch % 12;
            bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            g.setColour(juce::Colour(black ? 0xff202933 : 0xff26333e));
            g.fillRect(64, y, getWidth() - 64, 14);
            g.setColour(juce::Colour(black ? 0xff111923 : 0xffcad6dd));
            g.fillRect(0, y, 62, 13);
            g.setColour(black ? juce::Colour(0xffa4b6c5) : juce::Colour(0xff22313c));
            if (pc == 0)
                g.drawText("C" + juce::String(pitch / 12 - 1), 4, y, 54, 14, juce::Justification::centredRight);
        }
        g.setColour(juce::Colour(0xff344453));
        g.fillRect(0, 0, getWidth(), 32);
        if (clip.is_null())
        {
            g.setColour(juce::Colours::white);
            g.drawText(text("选择 MIDI 片段，或新增 MIDI / 乐器轨"), 72, 0, getWidth() - 80, 32,
                       juce::Justification::left);
            return;
        }
        for (const auto& line :
             grid(clip["start_samples"], clip["start_samples"].get<int64_t>() + clip["length_samples"].get<int64_t>(),
                  snap))
        {
            int x = 64 + int((line["beat"].get<double>() - clip["start_beat"].get<double>()) * scale());
            if (x < 64)
                continue;
            bool bar = line["bar_line"];
            g.setColour(juce::Colour(bar ? 0xff566c7c : 0xff344654));
            g.drawVerticalLine(x, 32, float(getHeight()));
            if (bar)
            {
                g.setColour(juce::Colour(0xffd4e3eb));
                g.drawText(juce::String(line["bar"].get<int>()) + " |", x + 4, 0, 70, 30, juce::Justification::left);
            }
        }
        for (const auto& n : clip["notes"])
            drawNote(g, n, selectedIDs.contains(n["id"].get<std::string>()) ? juce::Colour(0xfff1c975) : accent());
        if (dragging && !ghost.is_null())
        {
            g.setOpacity(0.55f);
            drawNote(g, ghost, juce::Colours::white);
            g.setOpacity(1);
        }
        const auto local = positionBeat - clip["start_beat"].get<double>();
        if (local >= 0 && local <= clip["length_beats"].get<double>())
        {
            g.setColour(juce::Colour(0xffedca72));
            g.drawVerticalLine(64 + int(local * scale()), 32, float(getHeight()));
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (playing || clip.is_null() || e.x < 64 || e.y < 32)
            return;
        grabKeyboardFocus();
        dragRevision = revision;
        dragClip = clip["id"];
        origin = e.position;
        dragging = false;
        creating = false;
        for (const auto& n : clip["notes"])
            if (noteBounds(n).contains(e.position))
            {
                const std::string key = n["id"];
                if (e.mods.isShiftDown())
                {
                    if (selectedIDs.contains(key))
                        selectedIDs.erase(key);
                    else
                        selectedIDs.insert(key);
                    selected = selectedIDs.contains(key) ? key : selectedIDs.empty() ? "" : *selectedIDs.begin();
                    ghost = nullptr;
                    selection();
                    repaint();
                    return;
                }
                selected = key;
                selectedIDs = {key};
                original = n;
                ghost = n;
                resizing = e.position.x > noteBounds(n).getRight() - 7;
                dragging = true;
                selection();
                repaint();
                return;
            }
        const auto local = std::floor((e.x - 64) / scale() / snap) * snap;
        if (local >= clip["length_beats"].get<double>())
            return;
        selected.clear();
        selectedIDs.clear();
        original = nullptr;
        creating = true;
        resizing = false;
        dragging = true;
        ghost = {{"id", ""},
                 {"start_beat", clip["start_beat"].get<double>() + local},
                 {"length_beats", std::min(snap, clip["length_beats"].get<double>() - local)},
                 {"pitch", pitchAt(e.y)},
                 {"velocity", 100}};
        selection();
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging || clip.is_null())
            return;
        const auto end = clip["start_beat"].get<double>() + clip["length_beats"].get<double>();
        if (creating || resizing)
        {
            const auto start = ghost["start_beat"].get<double>();
            const auto cursor = clip["start_beat"].get<double>() + std::round((e.x - 64) / scale() / snap) * snap;
            ghost["length_beats"] = std::clamp(cursor - start, std::min(snap, end - start), end - start);
        }
        else
        {
            const auto delta = std::round((e.x - origin.x) / scale() / snap) * snap;
            ghost["start_beat"] =
                std::clamp(original["start_beat"].get<double>() + delta, clip["start_beat"].get<double>(),
                           end - original["length_beats"].get<double>());
            ghost["pitch"] = std::clamp(original["pitch"].get<int>() + int(std::round((origin.y - e.y) / 14)), 0, 127);
        }
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (!dragging)
            return;
        dragging = false;
        auto changed = ghost;
        ghost = nullptr;
        repaint();
        if (!creating && changed == original)
            return;
        const auto start = sample(changed["start_beat"]),
                   end = sample(changed["start_beat"].get<double>() + changed["length_beats"].get<double>());
        Json a = {{"clip", dragClip},
                  {"pitch", changed["pitch"]},
                  {"velocity", changed["velocity"]},
                  {"position_samples", start},
                  {"length_samples", end - start}};
        if (!creating)
            a["note"] = changed["id"];
        write(creating ? "midi.note.add" : "midi.note.set", a, dragRevision);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.getTextCharacter() == 'a' && (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()))
        {
            selectAll();
            return true;
        }
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            removeSelected();
            return true;
        }
        return false;
    }

private:
    double scale() const
    {
        return clip.is_null() ? 72 : double(getWidth() - 64) / clip["length_beats"].get<double>();
    }
    static int pitchAt(int y)
    {
        return std::clamp(127 - (y - 32) / 14, 0, 127);
    }
    void drawNote(juce::Graphics& g, const Json& note, juce::Colour colour)
    {
        auto rect = noteBounds(note);
        g.setColour(colour);
        g.fillRoundedRectangle(rect, 2);
        g.setColour(colour.darker(.7f));
        g.fillRect(rect.withWidth(float(note["velocity"].get<int>() / 127.) * rect.getWidth()).removeFromBottom(2));
    }
    MusicalWriter write;
    std::function<int64_t(double)> sample;
    std::function<Json(int64_t, int64_t, double)> grid;
    std::function<void()> selection;
    Json clip = nullptr, original = nullptr, ghost = nullptr;
    std::string selected, dragClip;
    std::set<std::string> selectedIDs;
    uint64_t revision = 0, dragRevision = 0;
    bool playing = false, dragging = false, creating = false, resizing = false;
    double snap = .5, positionBeat = 0;
    juce::Point<float> origin;
};
class PianoRoll final : public juce::Component
{
public:
    PianoRoll(MusicalWriter writer, std::function<int64_t(double)> sample,
              std::function<Json(int64_t, int64_t, double)> grid, MusicalWriter transform)
        : canvas(std::move(writer), std::move(sample), grid, [this] { refreshSelection(); }), grid(std::move(grid)),
          transform(std::move(transform))
    {
        for (auto* c : std::initializer_list<juce::Component*>{
                 &view, &clips, &snap, &velocity, &remove, &detail, &selectionScope, &strength, &strengthLabel,
                 &quantize, &semitones, &transpose, &start, &end, &rangeLabel, &selectAll})
            addAndMakeVisible(c);
        view.setViewedComponent(&canvas, false);
        view.setScrollBarsShown(true, true);
        clips.setComponentID("midi.clip_choice");
        snap.setComponentID("midi.snap");
        velocity.setComponentID("midi.velocity");
        remove.setComponentID("midi.delete");
        snap.addItem(text("网格 1/4 拍"), 1);
        snap.addItem(text("网格 1/2 拍"), 2);
        snap.addItem(text("网格 1 拍"), 3);
        snap.setSelectedId(2, juce::dontSendNotification);
        velocity.setRange(1, 127, 1);
        velocity.setTextValueSuffix(text(" 力度"));
        velocity.setSliderStyle(juce::Slider::LinearHorizontal);
        velocity.setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 24);
        clips.onChange = [this]
        {
            chosen = clipIDs.at(size_t(clips.getSelectedId() - 1));
            refreshClip();
        };
        snap.onChange = [this] { refreshClip(); };
        velocity.onDragEnd = [this] { canvas.setVelocity(int(velocity.getValue())); };
        velocity.onValueChange = [this]
        {
            if (!velocity.isMouseButtonDown())
                canvas.setVelocity(int(velocity.getValue()));
        };
        remove.onClick = [this] { canvas.removeSelected(); };
        selectionScope.setComponentID("midi.transform.scope");
        selectionScope.addItem(text("所选音符"), 1);
        selectionScope.addItem(text("片段全部起音"), 2);
        selectionScope.addItem(text("采样区间起音"), 3);
        selectionScope.setSelectedId(1, juce::dontSendNotification);
        selectionScope.onChange = [this] { refreshSelection(); };
        strength.setComponentID("midi.quantize.strength");
        strength.setText("100", false);
        strength.setInputRestrictions(12);
        strengthLabel.setText(text("强度 %"), juce::dontSendNotification);
        semitones.setComponentID("midi.transpose.semitones");
        semitones.setText("12", false);
        semitones.setInputRestrictions(6);
        semitones.setTooltip(text("整数半音；越过 MIDI 0–127 会拒绝整笔计划"));
        start.setComponentID("midi.transform.start");
        end.setComponentID("midi.transform.end");
        start.setInputRestrictions(20);
        end.setInputRestrictions(20);
        rangeLabel.setText(text("区间 [采样起点, 终点)"), juce::dontSendNotification);
        quantize.setComponentID("midi.quantize.preview");
        transpose.setComponentID("midi.transpose.preview");
        selectAll.setComponentID("midi.select_all");
        selectAll.onClick = [this]
        {
            canvas.selectAll();
            selectionScope.setSelectedId(1, juce::dontSendNotification);
            refreshSelection();
        };
        quantize.onClick = [this] { requestTransform(true); };
        transpose.onClick = [this] { requestTransform(false); };
        detail.setFont(juce::FontOptions(12));
        setComponentID("midi.editor");
        view.setViewPosition(0, 32 + (127 - 84) * 14);
    }
    void update(const Json& track, uint64_t revision, bool playing, double positionBeat)
    {
        this->track = track;
        this->revision = revision;
        this->playing = playing;
        this->positionBeat = positionBeat;
        std::vector<std::string> ids;
        juce::StringArray names;
        if (!track.is_null())
            for (const auto& c : track["clips"])
                if (c["kind"] == "midi")
                {
                    ids.push_back(c["id"]);
                    names.add(text(c["name"].get<std::string>()));
                }
        if (ids != clipIDs)
        {
            clipIDs = ids;
            clips.clear(juce::dontSendNotification);
            for (int i = 0; i < names.size(); ++i)
                clips.addItem(names[i], i + 1);
        }
        if (std::find(ids.begin(), ids.end(), chosen) == ids.end())
            chosen = ids.empty() ? "" : ids.back();
        for (size_t i = 0; i < ids.size(); ++i)
            if (ids[i] == chosen)
                clips.setSelectedId(int(i) + 1, juce::dontSendNotification);
        refreshClip();
    }
    Json viewedClip() const
    {
        return currentClip;
    }
    void showClip(const std::string& id)
    {
        chosen = id;
        refreshClip();
    }
    std::function<void(std::string)> onError;
    void resized() override
    {
        clips.setBounds(10, 10, 160, 28);
        snap.setBounds(180, 10, 142, 28);
        velocity.setBounds(332, 10, 210, 28);
        remove.setBounds(552, 10, 140, 28);
        selectionScope.setBounds(10, 46, 148, 28);
        strengthLabel.setBounds(168, 46, 64, 28);
        strength.setBounds(232, 46, 48, 28);
        quantize.setBounds(290, 46, 112, 28);
        semitones.setBounds(412, 46, 50, 28);
        transpose.setBounds(472, 46, 112, 28);
        selectAll.setBounds(594, 46, 98, 28);
        rangeLabel.setBounds(10, 82, 180, 28);
        start.setBounds(190, 82, 152, 28);
        end.setBounds(352, 82, 152, 28);
        detail.setBounds(12, 116, getWidth() - 24, 26);
        view.setBounds(0, 184, getWidth(), std::max(0, getHeight() - 184));
        refreshClip();
        if (!scrolled && getHeight() > 222)
        {
            view.setViewPosition(0, 32 + (127 - 84) * 14);
            scrolled = true;
        }
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
        g.setColour(juce::Colour(0xff344453));
        g.fillRect(0, 152, getWidth(), 32);
        g.setColour(juce::Colour(0xffbfd1dc));
        g.setFont(juce::FontOptions(11));
        g.drawText("BARS", 4, 152, 56, 32, juce::Justification::centred);
        if (currentClip.is_null())
            return;
        const auto scale = (canvas.getWidth() - 64) / currentClip["length_beats"].get<double>();
        for (const auto& line :
             grid(currentClip["start_samples"],
                  currentClip["start_samples"].get<int64_t>() + currentClip["length_samples"].get<int64_t>(), 1))
        {
            const int x = 64 + int((line["beat"].get<double>() - currentClip["start_beat"].get<double>()) * scale) -
                          view.getViewPositionX();
            if (x < 64 || x > getWidth())
                continue;
            g.setColour(juce::Colour(0xff8299aa));
            g.drawVerticalLine(x, 172, 184);
            if (line["bar_line"])
            {
                g.setColour(juce::Colour(0xffd4e3eb));
                g.drawText(juce::String(line["bar"].get<int>()) + " |", x + 4, 152, 70, 32, juce::Justification::left);
            }
        }
    }

private:
    double division() const
    {
        return snap.getSelectedId() == 1 ? .25 : snap.getSelectedId() == 2 ? .5 : 1.;
    }
    void refreshClip()
    {
        Json clip = nullptr;
        if (!track.is_null())
            for (const auto& c : track["clips"])
                if (c["id"] == chosen)
                    clip = c;
        if (!clip.is_null() && (currentClip.is_null() || clip["id"] != currentClip["id"]))
        {
            start.setText(juce::String(clip["start_samples"].get<int64_t>()), false);
            end.setText(juce::String(clip["start_samples"].get<int64_t>() + clip["length_samples"].get<int64_t>()),
                        false);
        }
        currentClip = clip;
        canvas.update(clip, revision, playing, division(), std::max(100, view.getMaximumVisibleWidth()), positionBeat);
        refreshSelection();
        repaint();
    }
    void refreshSelection()
    {
        const auto n = canvas.selectedNote();
        bool enabled = !n.is_null() && !playing;
        velocity.setEnabled(enabled);
        remove.setEnabled(enabled);
        if (!n.is_null() && !velocity.isMouseButtonDown())
            velocity.setValue(n["velocity"].get<int>(), juce::dontSendNotification);
        const bool editable = !playing && !currentClip.is_null() && !currentClip["notes"].empty();
        selectionScope.setEnabled(editable);
        selectAll.setEnabled(editable);
        start.setEnabled(editable && selectionScope.getSelectedId() == 3);
        end.setEnabled(start.isEnabled());
        const bool hasSelection = selectionScope.getSelectedId() != 1 || !canvas.selectedNotes().empty(),
                   transformable = !currentClip.is_null() && currentClip.value("bulk_transform_available", false);
        quantize.setEnabled(editable && hasSelection && transformable);
        transpose.setEnabled(editable && hasSelection && transformable);
        strength.setEnabled(editable && transformable);
        semitones.setEnabled(editable && transformable);
        detail.setText(n.is_null() ? text("空白绘制 · Shift 点选 / ⌘A 全选 · 拖动当前音符 · 右缘改时长")
                                   : (text("已选 ") + juce::String(int(canvas.selectedNotes().size())) +
                                      text(" · 当前音高 ") + juce::String(n["pitch"].get<int>()) +
                                      text(" · 工程采样 ") + juce::String(n["position_samples"].get<int64_t>()) +
                                      text(" · ") + juce::String(n["length_beats"].get<double>(), 2) + text(" 拍")),
                       juce::dontSendNotification);
        if (!currentClip.is_null() && !transformable)
            detail.setText(text("当前片段的批量变换未验证：") +
                               text(currentClip["bulk_transform_restriction"].get<std::string>()),
                           juce::dontSendNotification);
    }
    static int64_t integer(const juce::TextEditor& input, bool signedValue)
    {
        const auto s = input.getText().toStdString();
        auto from = signedValue && !s.empty() && (s[0] == '-' || s[0] == '+') ? size_t(1) : size_t(0);
        if (from == s.size() || !std::all_of(s.begin() + from, s.end(), [](char c) { return c >= '0' && c <= '9'; }))
            throw std::runtime_error("请输入完整整数，采样位置不可为负数");
        size_t used = 0;
        auto n = std::stoll(s, &used);
        if (used != s.size())
            throw std::runtime_error("整数格式错误");
        return n;
    }
    void requestTransform(bool quantising)
    {
        try
        {
            if (currentClip.is_null() || playing)
                return;
            Json a = {{"clip", currentClip["id"]},
                      {"selection", selectionScope.getSelectedId() == 1   ? "notes"
                                    : selectionScope.getSelectedId() == 2 ? "all"
                                                                          : "range"}};
            if (selectionScope.getSelectedId() == 1)
                a["note_ids"] = canvas.selectedNotes();
            if (selectionScope.getSelectedId() == 3)
            {
                a["range_start_samples"] = integer(start, false);
                a["range_end_samples"] = integer(end, false);
            }
            if (quantising)
            {
                size_t used = 0;
                auto s = strength.getText().toStdString();
                double value = std::stod(s, &used);
                if (used != s.size() || !std::isfinite(value) || value < 0 || value > 100)
                    throw std::runtime_error("量化强度须为 0–100%");
                a["grid_beats"] = division();
                a["strength"] = value / 100.;
            }
            else
                a["semitones"] = integer(semitones, true);
            transform(quantising ? "midi.notes.quantize" : "midi.notes.transpose", a, revision);
        }
        catch (const std::exception& e)
        {
            if (onError)
                onError(e.what());
        }
    }
    NoteCanvas canvas;
    std::function<Json(int64_t, int64_t, double)> grid;
    MusicalWriter transform;
    juce::Viewport view;
    juce::ComboBox clips, snap, selectionScope;
    juce::Slider velocity;
    juce::TextButton remove{text("删除当前音符")}, quantize{text("预览量化")}, transpose{text("预览移调")},
        selectAll{text("全选音符")};
    juce::Label detail, strengthLabel, rangeLabel;
    juce::TextEditor strength, semitones, start, end;
    Json track = nullptr, currentClip = nullptr;
    std::vector<std::string> clipIDs;
    std::string chosen;
    uint64_t revision = 0;
    bool playing = false, scrolled = false;
    double positionBeat = 0;
};
