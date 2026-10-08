#pragma once
// Included by Workspace.h inside ndaw::desktop. This view only reads facts and emits L1 commands.
using MusicalBatchWriter = std::function<void(Json, uint64_t)>;
using MusicalWriter = std::function<void(const std::string&, Json, uint64_t)>;
class NoteCanvas final : public juce::Component
{
public:
    NoteCanvas(MusicalWriter writer, std::function<int64_t(double)> sample,
               std::function<Json(int64_t, int64_t, double)> grid, std::function<void()> selection,
               MusicalBatchWriter batch)
        : write(std::move(writer)), batch(std::move(batch)), sample(std::move(sample)), grid(std::move(grid)),
          selection(std::move(selection))
    {
        setWantsKeyboardFocus(true);
        setComponentID("midi.canvas");
    }
    void update(const Json& value, uint64_t revision, bool playing, double snap, int width, double positionBeat,
                const std::string& session = "", double zoom = 72.)
    {
        if (this->session != session || clip.is_null() != value.is_null() ||
            (!value.is_null() && (clip.is_null() || clip["id"] != value["id"])))
        {
            selected.clear();
            selectedIDs.clear();
            dragging = velocityDragging = creating = resizing = trimmingLeft = false;
            originals.clear();
            ghost = nullptr;
            ghosts.clear();
        }
        this->session = session;
        if (playing)
        {
            dragging = velocityDragging = creating = resizing = trimmingLeft = false;
            originals.clear();
            ghost = nullptr;
            ghosts.clear();
        }
        clip = value;
        this->revision = revision;
        this->playing = playing;
        this->snap = snap;
        this->zoom = zoom;
        this->positionBeat = positionBeat;
        std::set<std::string> valid;
        if (!clip.is_null())
            for (const auto& n : clip["notes"])
                if (selectedIDs.contains(n["id"].get<std::string>()))
                    valid.insert(n["id"]);
        selectedIDs = std::move(valid);
        if (selectedNote().is_null())
            selected = selectedIDs.empty() ? "" : *selectedIDs.begin();
        setSize(std::max(width, clip.is_null() ? width : 64 + int(clip["length_beats"].get<double>() * zoom)),
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
    void restoreSelection(const Json& ids)
    {
        std::set<std::string> next;
        if (!clip.is_null())
            for (const auto& n : clip["notes"])
                if (std::find(ids.begin(), ids.end(), n["id"]) != ids.end())
                    next.insert(n["id"]);
        if (next != selectedIDs)
        {
            selectedIDs = std::move(next);
            if (!selectedIDs.contains(selected))
                selected = selectedIDs.empty() ? "" : *selectedIDs.begin();
            selection();
            repaint();
        }
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
    bool editable() const
    {
        return !playing && !clip.is_null() && !clip.value("locked", false) &&
               clip.value("bulk_transform_available", false);
    }
    void commitNotes(const Json& notes, uint64_t version, bool deleting = false)
    {
        Json operations = Json::array();
        for (const auto& note : notes)
        {
            Json args{{"clip", dragClip.empty() ? clip["id"] : Json(dragClip)}, {"note", note["id"]}};
            if (!deleting)
            {
                const auto start = sample(note["start_beat"]);
                const auto end = sample(note["start_beat"].get<double>() + note["length_beats"].get<double>());
                args.update({{"pitch", note["pitch"]},
                             {"velocity", note["velocity"]},
                             {"position_samples", start},
                             {"length_samples", end - start}});
            }
            operations.push_back(operation(deleting ? "midi.note.delete" : "midi.note.set", args));
        }
        if (operations.empty())
            return;
        batch(operations, version);
    }
    Json selectedFacts() const
    {
        Json notes = Json::array();
        if (!clip.is_null())
            for (const auto& n : clip["notes"])
                if (selectedIDs.contains(n["id"].get<std::string>()))
                    notes.push_back(n);
        return notes;
    }
    void setVelocity(int velocity)
    {
        if (!editable())
            return;
        auto notes = selectedFacts();
        notes.erase(
            std::remove_if(notes.begin(), notes.end(), [velocity](const Json& n) { return n["velocity"] == velocity; }),
            notes.end());
        for (auto& note : notes)
            note["velocity"] = std::clamp(velocity, 1, 127);
        dragClip = clip["id"];
        commitNotes(notes, revision);
    }
    void removeSelected()
    {
        if (editable())
        {
            dragClip = clip["id"];
            commitNotes(selectedFacts(), revision, true);
        }
    }
    const Json& noteClip() const
    {
        return clip;
    }
    double pixelsPerBeat() const
    {
        return scale();
    }
    Json velocityFacts() const
    {
        auto notes = clip.is_null() ? Json::array() : clip["notes"];
        if (dragging && velocityDragging)
            for (auto& n : notes)
                for (const auto& preview : ghosts)
                    if (n["id"] == preview["id"])
                        n = preview;
        return notes;
    }
    bool beginVelocity(const std::string& key)
    {
        if (!editable())
            return false;
        for (const auto& n : clip["notes"])
            if (n["id"] == key)
            {
                if (!selectedIDs.contains(key))
                    selectedIDs = {key};
                selected = key;
                original = n;
                originals = selectedFacts();
                ghosts = originals;
                ghost = n;
                dragClip = clip["id"];
                dragRevision = revision;
                dragging = velocityDragging = true;
                creating = resizing = trimmingLeft = false;
                selection();
                return true;
            }
        return false;
    }
    void previewVelocity(int value)
    {
        if (!dragging || !velocityDragging)
            return;
        int delta = value - original["velocity"].get<int>();
        int minimum = 127, maximum = 1;
        for (const auto& n : originals)
        {
            minimum = std::min(minimum, n["velocity"].get<int>());
            maximum = std::max(maximum, n["velocity"].get<int>());
        }
        delta = std::clamp(delta, 1 - minimum, 127 - maximum);
        ghosts = originals;
        for (auto& n : ghosts)
            n["velocity"] = n["velocity"].get<int>() + delta;
        repaint();
    }
    void finishVelocity()
    {
        if (!velocityDragging)
            return;
        velocityDragging = false;
        const auto changed = ghosts;
        ghosts.clear();
        ghost = nullptr;
        dragging = false;
        if (changed != originals && editable())
            commitNotes(changed, dragRevision);
        repaint();
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
            if (creating)
                drawNote(g, ghost, juce::Colours::white);
            else
                for (const auto& n : ghosts)
                    drawNote(g, n, juce::Colours::white);
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
        if (!editable() || e.x < 64 || e.y < 32)
            return;
        grabKeyboardFocus();
        dragRevision = revision;
        dragClip = clip["id"];
        origin = e.position;
        dragging = false;
        creating = velocityDragging = trimmingLeft = false;
        ghosts.clear();
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
                if (!selectedIDs.contains(key))
                    selectedIDs = {key};
                original = n;
                ghost = n;
                originals = selectedFacts();
                ghosts = originals;
                const auto bounds = noteBounds(n);
                resizing = e.position.x > bounds.getRight() - std::min(7.f, bounds.getWidth() / 3);
                trimmingLeft = e.position.x < bounds.getX() + std::min(7.f, bounds.getWidth() / 3);
                velocityDragging = e.mods.isCommandDown();
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
        const auto clipStart = clip["start_beat"].get<double>();
        const auto end = clipStart + clip["length_beats"].get<double>();
        if (velocityDragging)
        {
            previewVelocity(original["velocity"].get<int>() + int(std::llround(origin.y - e.y)));
            return;
        }
        if (creating)
        {
            const auto start = ghost["start_beat"].get<double>();
            const auto cursor = clipStart + std::round((e.x - 64) / scale() / snap) * snap;
            ghost["length_beats"] = std::clamp(cursor - start, std::min(snap, end - start), end - start);
        }
        else
        {
            double delta = std::round((e.x - origin.x) / scale() / snap) * snap;
            double low = -1e9, high = 1e9;
            int pitchLow = -127, pitchHigh = 127;
            for (const auto& n : originals)
            {
                const double start = n["start_beat"], length = n["length_beats"];
                if (resizing)
                {
                    low = std::max(low, std::min(snap, length) - length);
                    high = std::min(high, end - start - length);
                }
                else if (trimmingLeft)
                {
                    low = std::max(low, clipStart - start);
                    high = std::min(high, length - std::min(snap, length));
                }
                else
                {
                    low = std::max(low, clipStart - start);
                    high = std::min(high, end - start - length);
                }
                pitchLow = std::max(pitchLow, -n["pitch"].get<int>());
                pitchHigh = std::min(pitchHigh, 127 - n["pitch"].get<int>());
            }
            delta = std::clamp(delta, low, high);
            const auto pitchDelta = std::clamp(int(std::round((origin.y - e.y) / 14)), pitchLow, pitchHigh);
            ghosts = originals;
            for (auto& n : ghosts)
            {
                if (resizing)
                    n["length_beats"] = n["length_beats"].get<double>() + delta;
                else if (trimmingLeft)
                {
                    n["start_beat"] = n["start_beat"].get<double>() + delta;
                    n["length_beats"] = n["length_beats"].get<double>() - delta;
                }
                else
                {
                    n["start_beat"] = n["start_beat"].get<double>() + delta;
                    n["pitch"] = n["pitch"].get<int>() + pitchDelta;
                }
            }
        }
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (!dragging)
            return;
        if (velocityDragging)
        {
            finishVelocity();
            return;
        }
        dragging = false;
        if (!creating)
        {
            auto changed = ghosts;
            ghosts.clear();
            ghost = nullptr;
            repaint();
            if (changed != originals && editable())
                commitNotes(changed, dragRevision);
            return;
        }
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
    std::function<bool(const juce::KeyPress&)> onCommandKey;
    bool keyPressed(const juce::KeyPress& key) override
    {
        return onCommandKey ? onCommandKey(key) : false;
    }

private:
    double scale() const
    {
        return zoom;
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
    MusicalBatchWriter batch;
    std::function<int64_t(double)> sample;
    std::function<Json(int64_t, int64_t, double)> grid;
    std::function<void()> selection;
    Json clip = nullptr, original = nullptr, ghost = nullptr, originals = Json::array(), ghosts = Json::array();
    std::string selected, dragClip, session;
    std::set<std::string> selectedIDs;
    uint64_t revision = 0, dragRevision = 0;
    bool playing = false, dragging = false, creating = false, resizing = false, trimmingLeft = false,
         velocityDragging = false;
    double snap = .5, positionBeat = 0, zoom = 72.;
    juce::Point<float> origin;
};
class VelocityLane final : public juce::Component
{
public:
    explicit VelocityLane(NoteCanvas& notes) : notes(notes)
    {
        setComponentID("midi.velocity_lane");
    }
    void setScroll(int offset)
    {
        scroll = offset;
        repaint();
    }
    float x(const Json& n) const
    {
        return notes.noteBounds(n).getX() - scroll;
    }
    float y(int velocity) const
    {
        return float(getHeight() - 8) - float(velocity) / 127.f * (getHeight() - 22);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff18212b));
        g.setColour(juce::Colour(0xff8299aa));
        g.drawText(text("力度"), 4, 4, 52, 18, juce::Justification::left);
        g.drawHorizontalLine(getHeight() - 8, 64.f, float(getWidth()));
        g.saveState();
        g.reduceClipRegion(64, 0, getWidth() - 64, getHeight());
        const auto selected = notes.selectedNotes();
        for (const auto& n : notes.velocityFacts())
        {
            g.setColour(std::find(selected.begin(), selected.end(), n["id"]) != selected.end()
                            ? juce::Colour(0xfff1c975)
                            : accent());
            g.drawLine(x(n), y(n["velocity"]), x(n), float(getHeight() - 8), 2.f);
            g.fillEllipse(x(n) - 4, y(n["velocity"]) - 4, 8, 8);
        }
        g.restoreState();
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        active = false;
        if (!notes.editable() || e.x < 64)
            return;
        Json chosen = nullptr;
        double nearest = 9.;
        for (const auto& n : notes.velocityFacts())
        {
            const auto distance = std::hypot(x(n) - e.x, y(n["velocity"]) - e.y);
            if (distance < nearest)
            {
                chosen = n;
                nearest = distance;
            }
        }
        if (!chosen.is_null())
        {
            active = notes.beginVelocity(chosen["id"]);
            if (active)
                notes.grabKeyboardFocus();
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (active)
        {
            notes.previewVelocity(
                std::clamp(int(std::llround((getHeight() - 8 - e.position.y) * 127. / (getHeight() - 22))), 1, 127));
            repaint();
        }
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (active)
            notes.finishVelocity();
        active = false;
        repaint();
    }

private:
    NoteCanvas& notes;
    int scroll = 0;
    bool active = false;
};
class MidiViewport final : public juce::Viewport
{
public:
    std::function<void()> onScroll;

private:
    void visibleAreaChanged(const juce::Rectangle<int>&) override
    {
        if (onScroll)
            onScroll();
    }
};
class PianoRoll final : public juce::Component
{
public:
    PianoRoll(MusicalWriter writer, std::function<int64_t(double)> sample,
              std::function<Json(int64_t, int64_t, double)> grid, MusicalWriter transform, MusicalBatchWriter batch)
        : canvas(
              writer, std::move(sample), grid,
              [this]
              {
                  refreshSelection();
                  if (!restoring && onSelection && !currentClip.is_null())
                      onSelection(currentClip, canvas.selectedNotes());
              },
              std::move(batch)),
          velocityLane(canvas), direct(std::move(writer)), grid(std::move(grid)), transform(std::move(transform))
    {
        for (auto* c : std::initializer_list<juce::Component*>{&view, &clips, &snap, &velocity, &remove, &detail,
                                                               &selectionScope, &strength, &strengthLabel, &quantize,
                                                               &semitones, &transpose, &start, &end, &rangeLabel,
                                                               &selectAll, &quickQuantize, &velocityLane, &advanced})
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
        view.onScroll = [this]
        {
            velocityLane.setScroll(view.getViewPositionX());
            if (!restoring && onView)
                onView({{"midi_scroll_x", view.getViewPositionX()}, {"midi_scroll_y", view.getViewPositionY()}});
            repaint();
        };
        advanced.setComponentID("midi.advanced");
        advanced.onClick = [this]
        {
            detailed = !detailed;
            resized();
        };
        clips.onChange = [this]
        {
            chosen = clipIDs.at(size_t(clips.getSelectedId() - 1));
            refreshClip();
            if (!restoring && onClip)
                onClip(chosen);
        };
        snap.onChange = [this]
        {
            refreshClip();
            if (!restoring && onView)
                onView({{"midi_grid_beats", division()}});
        };
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
        quickQuantize.setComponentID("midi.quantize.apply");
        quickQuantize.onClick = [this] { quantizeSelected(); };
        detail.setFont(juce::FontOptions(12));
        setComponentID("midi.editor");
        view.setViewPosition(0, 32 + (127 - 84) * 14);
    }
    void update(const Json& track, uint64_t revision, bool playing, double positionBeat,
                const std::string& session = "", const Json& ui = Json::object(), const Json& noteIDs = Json::array())
    {
        restoring = true;
        this->session = session;
        if (!ui.empty())
        {
            snap.setSelectedId(ui["midi_grid_beats"] == .25  ? 1
                               : ui["midi_grid_beats"] == .5 ? 2
                                                             : 3,
                               juce::dontSendNotification);
            zoom = ui["midi_pixels_per_beat"];
            const auto wanted = ui["midi_clip"].get<std::string>();
            if (!wanted.empty())
                chosen = wanted;
        }
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
        if (!ui.empty())
        {
            canvas.restoreSelection(noteIDs);
            view.setViewPosition(ui["midi_scroll_x"].get<int>(), ui["midi_scroll_y"].get<int>());
        }
        velocityLane.setScroll(view.getViewPositionX());
        restoring = false;
    }
    bool canQuantize() const
    {
        return canvas.editable() && !canvas.selectedNotes().empty();
    }
    void quantizeSelected()
    {
        if (!canQuantize())
            return;
        try
        {
            direct("midi.notes.quantize",
                   {{"clip", currentClip["id"]},
                    {"selection", "notes"},
                    {"note_ids", canvas.selectedNotes()},
                    {"grid_beats", division()},
                    {"strength", strengthValue()}},
                   revision);
        }
        catch (const std::exception& e)
        {
            if (onError)
                onError(e.what());
        }
    }
    void selectNotes()
    {
        canvas.selectAll();
    }
    void deleteNotes()
    {
        canvas.removeSelected();
    }
    void velocityStep(int delta)
    {
        const auto n = canvas.selectedNote();
        if (!n.is_null() && canvas.beginVelocity(n["id"]))
        {
            canvas.previewVelocity(n["velocity"].get<int>() + delta);
            canvas.finishVelocity();
        }
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        canvas.onCommandKey = [&manager, this](const juce::KeyPress& key)
        { return manager.getKeyMappings()->keyPressed(key, &canvas); };
        quickQuantize.setCommandToTrigger(&manager, 140, true);
        selectAll.setCommandToTrigger(&manager, 141, true);
        remove.setCommandToTrigger(&manager, 142, true);
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
    std::function<void(Json)> onView;
    std::function<void(const Json&, const Json&)> onSelection;
    std::function<void(const std::string&)> onClip;
    bool editorHasFocus() const
    {
        return hasKeyboardFocus(true);
    }
    void focusEditor()
    {
        if (isShowing())
            canvas.grabKeyboardFocus();
    }
    void scrollTo(int x, int y)
    {
        view.setViewPosition(x, y);
    }
    int scrollX() const
    {
        return view.getViewPositionX();
    }
    int scrollY() const
    {
        return view.getViewPositionY();
    }

    void resized() override
    {
        restoring = true;
        const double ratio = std::min(1., std::max(.5, (getWidth() - 20.) / 622.));
        auto place = [ratio](juce::Component& c, int x, int y, int width)
        { c.setBounds(10 + int((x - 10) * ratio), y, int(width * ratio), 28); };
        place(clips, 10, 6, 140);
        place(snap, 158, 6, 110);
        place(quickQuantize, 276, 6, 90);
        place(velocity, 374, 6, 190);
        place(advanced, 572, 6, 60);
        for (auto* c : std::initializer_list<juce::Component*>{&remove, &selectionScope, &strengthLabel, &strength,
                                                               &quantize, &semitones, &transpose, &selectAll})
            c->setVisible(detailed);
        place(selectionScope, 10, 42, 148);
        place(strengthLabel, 166, 42, 60);
        place(strength, 226, 42, 48);
        place(quantize, 282, 42, 100);
        place(semitones, 390, 42, 42);
        place(transpose, 440, 42, 94);
        place(remove, 542, 42, 90);
        place(selectAll, 10, 78, 96);
        place(rangeLabel, 114, 78, 160);
        place(start, 282, 78, 156);
        place(end, 446, 78, 186);
        detail.setBounds(12, detailed ? 112 : 36, getWidth() - 24, 22);
        rulerTop = detailed ? 138 : 60;
        noteTop = rulerTop + 24;
        view.setBounds(0, noteTop, getWidth(), std::max(0, getHeight() - noteTop - 68));
        velocityLane.setBounds(0, std::max(noteTop, getHeight() - 64), getWidth(), 60);
        refreshClip();
        velocityLane.setScroll(view.getViewPositionX());
        restoring = false;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
        g.setColour(juce::Colour(0xff344453));
        g.fillRect(0, rulerTop, getWidth(), 24);
        g.setColour(juce::Colour(0xffbfd1dc));
        g.setFont(juce::FontOptions(11));
        g.drawText("BARS", 4, rulerTop, 56, 24, juce::Justification::centred);
        if (currentClip.is_null())
            return;
        const auto scale = canvas.pixelsPerBeat();
        for (const auto& line :
             grid(currentClip["start_samples"],
                  currentClip["start_samples"].get<int64_t>() + currentClip["length_samples"].get<int64_t>(), 1))
        {
            const int x = 64 + int((line["beat"].get<double>() - currentClip["start_beat"].get<double>()) * scale) -
                          view.getViewPositionX();
            if (x < 64 || x > getWidth())
                continue;
            g.setColour(juce::Colour(0xff8299aa));
            g.drawVerticalLine(x, rulerTop + 12, float(noteTop));
            if (line["bar_line"])
            {
                g.setColour(juce::Colour(0xffd4e3eb));
                g.drawText(juce::String(line["bar"].get<int>()) + " |", x + 4, rulerTop, 70, 24,
                           juce::Justification::left);
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
        canvas.update(clip, revision, playing, division(), std::max(100, view.getMaximumVisibleWidth()), positionBeat,
                      session, zoom);
        refreshSelection();
        repaint();
    }
    void refreshSelection()
    {
        velocityLane.repaint();
        quickQuantize.setEnabled(canQuantize());
        const auto n = canvas.selectedNote();
        bool enabled = !n.is_null() && canvas.editable();
        velocity.setEnabled(enabled);
        remove.setEnabled(enabled);
        if (!n.is_null() && !velocity.isMouseButtonDown())
            velocity.setValue(n["velocity"].get<int>(), juce::dontSendNotification);
        const bool editable = !playing && !currentClip.is_null() && !currentClip["notes"].empty();
        selectionScope.setEnabled(editable);
        selectAll.setEnabled(editable);
        const bool rangeMode = selectionScope.getSelectedId() == 3;
        start.setVisible(rangeMode && detailed);
        end.setVisible(rangeMode && detailed);
        rangeLabel.setVisible(rangeMode && detailed);
        start.setEnabled(editable && rangeMode);
        end.setEnabled(start.isEnabled());
        const bool hasSelection = selectionScope.getSelectedId() != 1 || !canvas.selectedNotes().empty(),
                   transformable = !currentClip.is_null() && currentClip.value("bulk_transform_available", false);
        quantize.setEnabled(editable && hasSelection && transformable);
        transpose.setEnabled(editable && hasSelection && transformable);
        strength.setEnabled(editable && transformable);
        semitones.setEnabled(editable && transformable);
        detail.setText(n.is_null() ? text("空白绘制 · Shift 点选 / ⌘A 全选 · 拖动所选音符 · 两缘改时长 · ⌘拖改力度")
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
    double strengthValue() const
    {
        size_t used = 0;
        auto s = strength.getText().toStdString();
        double value = std::stod(s, &used);
        if (used != s.size() || !std::isfinite(value) || value < 0 || value > 100)
            throw std::runtime_error("量化强度须为 0–100%");
        return value / 100.;
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
                a["grid_beats"] = division();
                a["strength"] = strengthValue();
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
    VelocityLane velocityLane;
    MusicalWriter direct;
    std::function<Json(int64_t, int64_t, double)> grid;
    MusicalWriter transform;
    MidiViewport view;
    juce::ComboBox clips, snap, selectionScope;
    juce::Slider velocity;
    juce::TextButton remove{text("删除所选音符")}, quantize{text("预览量化")}, transpose{text("预览移调")},
        selectAll{text("全选音符")}, quickQuantize{text("量化所选")}, advanced{text("变换…")};
    juce::Label detail, strengthLabel, rangeLabel;
    juce::TextEditor strength, semitones, start, end;
    Json track = nullptr, currentClip = nullptr;
    std::vector<std::string> clipIDs;
    std::string chosen, session;
    uint64_t revision = 0;
    bool playing = false, restoring = false, detailed = false;
    int rulerTop = 60, noteTop = 84;
    double positionBeat = 0, zoom = 72.;
};
