#pragma once
#include "Theme.h"
#include "TrackHeader.h"
#include "TrackPresentation.h"
#include "EditWindowViews.h"
#include "Waveforms.h"
#include "Rulers.h"
#include "EditingModel.h"
#include "AutomationLane.h"
#include "ZoomGesture.h"
#include "ScrubGesture.h"
#include "TimeSelectionGesture.h"
namespace ndaw::desktop
{
class EditWindow final : public juce::Component, private juce::ScrollBar::Listener
{
public:
    EditWindow(Writer write, std::function<void(std::string)> select, std::function<void(int64_t)> seek,
               Waveforms& waves, std::function<void(std::string)> openMidi,
               std::function<void(std::string)> selectClip = {},
               std::function<void(const std::string&, Json, uint64_t)> clipWrite = {})
        : write(std::move(write)), select(std::move(select)), seek(std::move(seek)), waves(waves),
          openMidi(std::move(openMidi)), selectClip(std::move(selectClip)), clipWrite(std::move(clipWrite))
    {
        setComponentID("edit.timeline");
        addAndMakeVisible(waveformControls);
        rulerSelector.setComponentID("rulers.menu");
        rulerSelector.setButtonText(text("标尺"));
        rulerSelector.setTooltip(text("标尺显示 / 主时间标尺 / 时间码显示帧率"));
        rulerSelector.onClick = [this]
        {
            if (onRulersMenu)
                onRulersMenu(rulerSelector);
        };
        addAndMakeVisible(rulerSelector);
        horizontal.setComponentID("timeline.scroll.horizontal");
        vertical.setComponentID("timeline.scroll.vertical");
        horizontal.setAutoHide(false);
        vertical.setAutoHide(false);
        horizontal.addListener(this);
        vertical.addListener(this);
        addAndMakeVisible(horizontal);
        addAndMakeVisible(vertical);
    }
    void update(const Json& value, const std::string& selection, const Json& grid,
                const std::string& clipSelection = {}, const Json& rulerContext = Json::object())
    {
        if (!drag.is_null() && drag.value("session", std::string{}) != value.value("session_token", std::string{}))
            drag = nullptr;
        if (!resizeTrack.empty() && resizeSession != value.value("session_token", std::string{}))
            cancelHeightPreview();
        if (zoomGesture.active && (zoomGesture.session != value.value("session_token", std::string{}) ||
                                   zoomGesture.revision != value.value("revision", uint64_t(0))))
            zoomGesture.cancel();
        if (scrubGesture && (value.value("session_token", std::string{}) != scrubSession ||
                             value.value("revision", uint64_t(0)) != scrubRevision ||
                             !value.value("scrub", Json::object()).value("busy", false)))
        {
            const auto reason = value.value("scrub", Json::object()).value("reason", std::string{});
            cancelScrubGesture();
            if (onScrubStopped && !reason.empty())
                onScrubStopped(reason);
        }
        if (scrubGesture && scrubPreparing && value.value("scrub", Json::object()).value("active", false))
        {
            scrubPreparing = false;
            if (onScrubReady)
                onScrubReady();
        }
        if (scrubGesture)
        {
            const bool waiting = value.value("scrub", Json::object()).value("cache_waiting", false);
            if (waiting != scrubBuffering)
            {
                scrubBuffering = waiting;
                if (onScrubBuffering)
                    onScrubBuffering(waiting);
            }
        }
        facts = value;
        facts["tracks"] = Json::array();
        for (const auto& t : value["tracks"])
            if (!t.value("edit_hidden", false))
                facts["tracks"].push_back(t);
        selected = selection;
        selectedClip = clipSelection;
        this->grid = grid;
        this->rulerContext = rulerContext;
        std::vector<std::string> ids;
        for (const auto& t : facts["tracks"])
            ids.push_back(t["id"]);
        if (ids != trackIDs)
        {
            cancelHeightPreview();
            trackIDs = ids;
            controls.clear();
            columns.clear();
            automationLanes.clear();
            for (auto& id : ids)
            {
                auto c = std::make_unique<TrackHeader>(id, false, write, select);
                c->onRecordingCommand = [this](auto id, int command)
                {
                    if (onRecordingCommand)
                        onRecordingCommand(id, command);
                };
                c->onMonitorMenu = [this](auto id, auto& component)
                {
                    if (onMonitorMenu)
                        onMonitorMenu(id, component);
                };
                c->onOptions = [this](auto id, auto& component, bool strip)
                {
                    if (onTrackOptions)
                        onTrackOptions(id, component, strip);
                };
                c->onView = [this](auto id, auto parameter)
                {
                    if (onTrackView)
                        onTrackView(id, parameter);
                };
                auto lane = std::make_unique<AutomationLane>();
                lane->onCommit = [this](auto ops, auto revision, auto session)
                {
                    if (onAutomationCommit)
                        onAutomationCommit(ops, revision, session);
                };
                lane->onSelection = [this](auto ref)
                {
                    if (onAutomationSelection)
                        onAutomationSelection(ref);
                };
                lane->onSnap = [this](auto sample, auto beats) { return onSnap ? onSnap(sample, beats) : sample; };
                lane->onError = [this](auto error)
                {
                    if (onAutomationError)
                        onAutomationError(error);
                };
                lane->onRangeEvent = [this, canvas = lane.get()](int action, const juce::MouseEvent& e)
                {
                    const auto event = e.getEventRelativeTo(this);
                    if (action == 0)
                        mouseDown(event);
                    else if (action == 1)
                        mouseDrag(event);
                    else
                        mouseUp(event);
                    if (!drag.is_null() && drag.value("mode", std::string{}) == "selection")
                        canvas->previewRange({{"start_samples", std::min(dragStart, dragEnd)},
                                              {"end_samples", std::max(dragStart, dragEnd)}});
                };
                addChildComponent(*lane);
                automationLanes.push_back(std::move(lane));
                c->onHeight = [this](auto id, int height, bool finished)
                {
                    if (resizeTrack.empty())
                        resizeSession = facts.value("session_token", std::string{});
                    resizeTrack = id;
                    resizeHeight = height;
                    if (finished)
                    {
                        const auto session = resizeSession;
                        resizeTrack.clear();
                        if (onTrackHeight)
                            onTrackHeight(id, height, session);
                    }
                    resized();
                    repaint();
                };
                addAndMakeVisible(*c);
                controls.push_back(std::move(c));
                auto views = std::make_unique<EditWindowViews>(
                    id,
                    [this](auto t, int slot)
                    {
                        if (onInsert)
                            onInsert(t, slot);
                    },
                    [this](auto t, bool input)
                    {
                        if (onRouting)
                            onRouting(t, input);
                    },
                    [this](auto t, auto send)
                    {
                        if (onSend)
                            onSend(t, send);
                    },
                    [this](auto t)
                    {
                        if (onComments)
                            onComments(t);
                    });
                addAndMakeVisible(*views);
                columns.push_back(std::move(views));
            }
        }
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto item = facts["tracks"][i];
            item["playing"] = facts["playing"];
            item["recording_controls_pending"] = TrackRecordingState::blocked(facts);
            item["automation_writing"] = !facts["automation_capture"].is_null();
            item["recording"] = !facts["recording_capture"].is_null();
            controls[i]->update(item, trackIDs[i] == selected);
            columns[i]->update(item, value["tracks"]);
        }
        std::set<std::string> live;
        for (const auto& t : facts["tracks"])
            for (const auto& c : t["clips"])
            {
                auto id = c["id"].get<std::string>();
                live.insert(id);
                if (!headers.contains(id))
                {
                    auto b = std::make_unique<juce::TextButton>();
                    b->setComponentID("clip.select:" + text(id));
                    b->setInterceptsMouseClicks(!ZoomGesture::isTool(editing.tool) && editing.tool != "scrubber",
                                                false);
                    b->onClick = [this, id, owner = t["id"].get<std::string>()]
                    {
                        if (onClipSelection)
                            onClipSelection(id, juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown());
                        else
                        {
                            select(owner);
                            if (selectClip)
                                selectClip(id);
                        }
                    };
                    b->setTooltip(text("选择片段 · 使用移动工具拖动，修剪工具调整边界"));
                    addAndMakeVisible(*b);
                    headers[id] = std::move(b);
                }
                auto& b = *headers.at(id);
                b.setButtonText(text(c["name"].get<std::string>()) +
                                (c.value("locked", false) ? text("  🔒") : text("")));
                b.setColour(juce::TextButton::buttonColourId, trackColour(t).darker(id == selectedClip ? .25f : .6f));
            }
        for (auto it = headers.begin(); it != headers.end();)
            if (!live.contains(it->first))
                it = headers.erase(it);
            else
                ++it;
        resized();
        updateAutomation();
        repaint();
    }
    void setView(const Json& value)
    {
        if (scrubGesture)
            for (const auto* key : {"edit_tool", "start_samples", "span_samples", "first_row", "rulers", "edit_views",
                                    "track_heights", "track_views"})
                if (view.value(key, Json(nullptr)) != value.value(key, Json(nullptr)))
                {
                    cancelScrubGesture();
                    break;
                }
        if (!drag.is_null())
            for (const auto* key : {"start_samples", "span_samples", "first_row", "row_height", "edit_views", "rulers",
                                    "main_time_scale", "track_heights", "track_views", "waveform_zoom"})
                if (view.value(key, Json(nullptr)) != value.value(key, Json(nullptr)))
                {
                    drag = nullptr;
                    dragged = false;
                    break;
                }
        if (!resizeTrack.empty())
            for (const auto* key :
                 {"track_heights", "row_height", "first_row", "start_samples", "span_samples", "rulers", "edit_views"})
                if (view.value(key, Json(nullptr)) != value.value(key, Json(nullptr)))
                {
                    cancelHeightPreview();
                    break;
                }
        if (zoomGesture.active)
            for (const auto* key : {"start_samples", "span_samples", "first_row", "row_height", "edit_views", "rulers",
                                    "track_heights", "track_views", "waveform_zoom"})
                if (view.value(key, Json(nullptr)) != value.value(key, Json(nullptr)))
                    zoomGesture.cancel();
        view = value;
        resized();
        repaint();
    }
    void setModels(const EditingModel& tools, const SelectionModel& selectedObjects)
    {
        if (!drag.is_null() && drag.value("mode", std::string{}) == "selection" &&
            (selectedObjects.tracks != selection.tracks || selectedObjects.objects != selection.objects ||
             editing.gridBeats != tools.gridBeats))
            cancelTimeSelection();
        if (editing.tool != tools.tool || editing.mode != tools.mode)
        {
            drag = nullptr;
            dragged = false;
            zoomGesture.cancel();
        }
        editing = tools;
        for (auto& [id, header] : headers)
            header->setInterceptsMouseClicks(!ZoomGesture::isTool(editing.tool) && editing.tool != "scrubber", false);
        selection = selectedObjects;
    }
    std::function<bool(const std::string&, const Json&)> onScrub;
    std::function<void(const std::string&)> onScrubStopped;
    std::function<void()> onScrubReady;
    std::function<void(bool)> onScrubBuffering;
    std::function<void(Json, std::string, uint64_t)> onZoomGesture;
    std::function<void(std::string, juce::Component&, bool)> onTrackOptions;
    std::function<void(std::string, int)> onRecordingCommand;
    std::function<void(std::string, juce::Component&)> onMonitorMenu;
    std::function<void(std::string, int, std::string)> onTrackHeight;
    std::function<void(std::string, std::string)> onTrackView;
    std::function<Json(std::string)> onAutomationQuery;
    std::function<Json(std::string, std::string, int64_t, int64_t)> onAutomationSamples;
    std::function<void(Json, uint64_t, std::string)> onAutomationCommit;
    std::function<void(Json)> onAutomationSelection;
    std::function<void(std::string)> onAutomationError;
    std::function<void(juce::Component&)> onRulersMenu;
    std::function<void(int)> onRulerCommand;
    std::function<void(Json, uint64_t)> onLoopRange;
    int rulerHeight() const
    {
        return Rulers::height(view);
    }
    int markerLaneY() const
    {
        return Rulers::top(view, "markers");
    }
    juce::Rectangle<int> loopHandleRect(bool start) const
    {
        const auto settings = facts.value("transport_settings", Json::object());
        auto loop = settings.value("loop_range", Json(nullptr));
        if (loop.is_null() || !settings.value("loop_enabled", false))
            return {};
        if (!drag.is_null() && (drag["mode"] == "loop_start" || drag["mode"] == "loop_end"))
            loop = {{"start_samples", dragStart}, {"end_samples", dragEnd}};
        const int x =
            int(std::clamp(coordinates().pixelAt(loop[start ? "start_samples" : "end_samples"]), -1000000., 1000000.));
        return {x - 5, Rulers::top(view, view.value("main_time_scale", std::string("min_sec"))) + 18, 11, 11};
    }
    std::function<void(Json)> onViewChange;
    std::function<int64_t(int64_t, double)> onSnap;
    std::function<void(std::string, bool)> onClipSelection;
    std::function<void(const std::string&)> onMarkerClick;
    std::function<void(std::string)> onContext;
    std::function<void(Json, Json, uint64_t, std::string, int64_t)> onRange;
    std::function<void(std::string)> onComments;
    std::function<void(std::string, int)> onInsert;
    std::function<void(std::string, bool)> onRouting;
    std::function<void(std::string, std::string)> onSend;
    int columnCount() const
    {
        int count = 0;
        for (const auto* key : {"io", "inserts", "sends", "comments"})
            if (view.value("edit_views", Json::object()).value(key, false))
                ++count;
        return count;
    }
    int columnWidth() const
    {
        return columnCount() == 0 ? 0 : std::clamp((getWidth() - 250 - 16 - 160) / columnCount(), 48, 104);
    }
    int timelineLeft() const
    {
        return 250 + columnCount() * columnWidth();
    }
    void connectWaveformZoom(juce::ApplicationCommandManager& manager)
    {
        waveformControls.connect(manager);
    }
    double waveformDisplayScale(const std::string& track) const
    {
        const auto& draft = zoomGesture.draft();
        return waveformScale(zoomGesture.active && draft.contains("waveform_zoom") ? draft["waveform_zoom"]
                                                                                   : view["waveform_zoom"],
                             track);
    }
    TimelineCoordinates coordinates() const
    {
        const auto& draft = zoomGesture.draft();
        const auto start = zoomGesture.active && draft.contains("start_samples")
                               ? draft["start_samples"].get<int64_t>()
                               : view.value("start_samples", int64_t(0));
        const auto span = zoomGesture.active && draft.contains("span_samples")
                              ? draft["span_samples"].get<int64_t>()
                              : view.value("span_samples", int64_t(480000));
        return {start, span, double(timelineLeft()), double(std::max(1, getWidth() - timelineLeft() - 16))};
    }
    int rowY(int row) const
    {
        const int first = std::clamp(view.value("first_row", 0), 0, std::max(0, visibleRows() - 1));
        return rulerHeight() + (row >= 0 && row < int(rowOffsets.size()) ? rowOffsets[size_t(row)] : 0) -
               (first < int(rowOffsets.size()) ? rowOffsets[size_t(first)] : 0);
    }
    int rowHeight(int row) const
    {
        if (row < 0 || row >= int(trackIDs.size()))
            return view.value("row_height", 144);
        const auto& id = trackIDs[size_t(row)];
        return id == resizeTrack ? resizeHeight : TrackPresentation::height(view, id);
    }
    int visibleRows() const
    {
        return int(trackIDs.size());
    }
    double duration() const
    {
        return coordinates().span / 48000.;
    }
    juce::Rectangle<int> clipRect(const Json& c, int row) const
    {
        const auto axis = coordinates();
        const int left = int(std::clamp(axis.pixelAt(c["start_samples"]), -1000000., 1000000.));
        const int right = int(std::clamp(
            axis.pixelAt(c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>()), -1000000., 1000000.));
        const bool micro = rowHeight(row) < 64;
        return {left, rowY(row) + (micro ? 4 : 12), std::max(3, right - left), rowHeight(row) - (micro ? 8 : 26)};
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
        const auto axis = coordinates();
        Rulers::draw(g, axis, grid, getWidth() - 16, view, facts, rulerContext);
        if (markerLaneY() >= 0)
        {
            juce::Graphics::ScopedSaveState markerState(g);
            g.reduceClipRegion(int(axis.left), markerLaneY(), std::max(1, getWidth() - int(axis.left) - 16), 20);
            for (const auto& marker : facts.value("markers", Json::array()))
            {
                const int x = int(std::round(axis.pixelAt(marker["position_samples"])));
                if (x < int(axis.left) - 120 || x > getWidth() - 14)
                    continue;
                const bool range = marker.value("kind", std::string{}) == "selection";
                const auto colour = range ? juce::Colour(0xff73c9b7) : juce::Colour(0xffe8c36c);
                g.setColour(colour.withAlpha(range ? .38f : .95f));
                if (range)
                    g.fillRoundedRectangle(
                        float(x), float(markerLaneY() + 3),
                        float(std::max<int64_t>(
                            3, std::llround(marker.value("length_samples", int64_t(0)) * axis.width / axis.span))),
                        14.f, 3.f);
                juce::Path flag;
                flag.startNewSubPath(float(x), float(markerLaneY() + 2));
                flag.lineTo(float(x), float(markerLaneY() + 16));
                flag.lineTo(float(x + 6), float(markerLaneY() + 11));
                flag.lineTo(float(x), float(markerLaneY() + 8));
                flag.closeSubPath();
                g.fillPath(flag);
                g.setColour(juce::Colour(0xfff1f3f5));
                g.setFont(juce::FontOptions(10, juce::Font::bold));
                g.drawFittedText(text(marker.value("name", std::string{})), x + 8, markerLaneY() + 1, 134, 20 - 2,
                                 juce::Justification::centredLeft, 1);
            }
        }
        const auto leftHandle = loopHandleRect(true), rightHandle = loopHandleRect(false);
        if (!leftHandle.isEmpty())
        {
            juce::Graphics::ScopedSaveState loopState(g);
            g.reduceClipRegion(int(axis.left), 0, std::max(1, getWidth() - int(axis.left) - 16), rulerHeight());
            g.setColour(accent().withAlpha(.5f));
            g.fillRect(leftHandle.getCentreX(), leftHandle.getY() + 5,
                       std::max(1, rightHandle.getCentreX() - leftHandle.getCentreX()), 3);
            g.setColour(accent());
            for (auto handle : {leftHandle, rightHandle})
                g.fillRect(handle);
        }
        juce::Graphics::ScopedSaveState clipState(g);
        g.reduceClipRegion(juce::Rectangle<int>(timelineLeft(), rulerHeight(),
                                                std::max(1, getWidth() - timelineLeft() - 16),
                                                std::max(1, getHeight() - rulerHeight() - 16)));
        for (size_t i = 0; i < facts.value("tracks", Json::array()).size(); ++i)
        {
            g.setColour(juce::Colour(facts["tracks"][i]["id"] == selected ? 0xff21313d : 0xff1c2530));
            g.fillRect(timelineLeft(), rowY(int(i)), getWidth() - timelineLeft(), rowHeight(int(i)) - 1);
        }
        for (const auto& line : grid)
        {
            int x = int(std::round(axis.pixelAt(line["samples"])));
            bool bar = line["bar_line"];
            g.setColour(juce::Colour(bar ? 0xff536575 : 0xff303b49));
            g.drawVerticalLine(x, rulerHeight(), float(getHeight()));
        }
        auto drawSelection = [&](const Json& range, const Json& owners)
        {
            if (range.is_null())
                return;
            const auto left = int(std::clamp(axis.pixelAt(range["start_samples"]), -1000000., 1000000.)),
                       right = int(std::clamp(axis.pixelAt(range["end_samples"]), -1000000., 1000000.));
            for (size_t row = 0; row < trackIDs.size(); ++row)
                if (owners.empty() || std::find(owners.begin(), owners.end(), trackIDs[row]) != owners.end())
                {
                    g.setColour(accent().withAlpha(.16f));
                    g.fillRect(left, rowY(int(row)), std::max(1, right - left), rowHeight(int(row)));
                }
            g.setColour(accent());
            g.drawVerticalLine(left, rulerHeight(), float(getHeight()));
            g.drawVerticalLine(right, rulerHeight(), float(getHeight()));
        };
        for (size_t i = 0; i < facts.value("tracks", Json::array()).size(); ++i)
        {
            int y = rowY(int(i));
            const auto& t = facts["tracks"][i];
            g.setColour(juce::Colour(0xff33404e));
            g.drawHorizontalLine(y + rowHeight(int(i)) - 1, 0, float(getWidth()));
            if (!viewParameter(t["id"]).empty())
                continue;
            for (const auto& c : t["clips"])
            {
                auto rect = clipRect(c, int(i));
                double start = c["start_samples"].get<int64_t>() / 48000.,
                       length = c["length_samples"].get<int64_t>() / 48000.;
                g.setColour(trackColour(t).darker(t["audible"].get<bool>() ? .45f : .8f));
                g.fillRoundedRectangle(rect.toFloat(), 3);
                g.setColour(t["audible"].get<bool>() ? juce::Colour(0xffa2dcd6) : juce::Colour(0xff738995));
                if (c.value("kind", std::string{}) == "midi")
                    for (const auto& n : c["notes"])
                    {
                        const double p = (n["position_samples"].get<int64_t>() / 48000. - start) / length,
                                     l = n["length_samples"].get<int64_t>() / 48000. / length;
                        g.fillRect(rect.getX() + 4 + int(p * (rect.getWidth() - 8)),
                                   rect.getY() + 28 + int((127 - n["pitch"].get<int>()) / 127. * 72),
                                   std::max(2, int(l * (rect.getWidth() - 8))), 3);
                    }
                else
                {
                    auto waveRect = rect.reduced(0, 25).getIntersection(
                        juce::Rectangle<int>(timelineLeft(), rulerHeight(), getWidth() - timelineLeft() - 16,
                                             getHeight() - rulerHeight() - 16));
                    if (!waveRect.isEmpty())
                    {
                        const auto elapsed =
                            std::max(0., (axis.sampleAt(waveRect.getX()) - c["start_samples"].get<int64_t>()) / 48000.);
                        waves.draw(g, c, waveRect, waveRect.getWidth() / axis.width * axis.span / 48000., elapsed,
                                   waveformDisplayScale(t["id"]));
                    }
                    g.setColour(juce::Colour(0xffe6e1b2));
                    auto fadeIn = c.value("fade_in_samples", int64_t(0));
                    auto fadeOut = c.value("fade_out_samples", int64_t(0));
                    if (!drag.is_null() && drag.value("clip_id", std::string{}) == c["id"].get<std::string>() &&
                        (drag["mode"] == "fade_in" || drag["mode"] == "fade_out"))
                    {
                        fadeIn = drag.value("preview_fade_in", fadeIn);
                        fadeOut = drag.value("preview_fade_out", fadeOut);
                    }
                    double in = fadeIn / 48000. / length, out = fadeOut / 48000. / length;
                    drawFade(g, rect, in, c.value("fade_in_curve", std::string("linear")), true);
                    drawFade(g, rect, out, c.value("fade_out_curve", std::string("linear")), false);
                    if (editing.tool == "smart" && c.value("editable_audio", false) && !c.value("locked", false) &&
                        rect.getWidth() > 24)
                    {
                        g.setColour(juce::Colour(0xfff0d283));
                        for (const auto handleX : {rect.getX() + int(std::llround(in * rect.getWidth())),
                                                   rect.getRight() - int(std::llround(out * rect.getWidth()))})
                            g.fillEllipse(float(handleX - 3), float(rect.getY() + 24), 7.f, 7.f);
                    }
                }
                if (selection.contains(c["id"]) || c["id"] == selectedClip)
                {
                    g.setColour(accent());
                    g.drawRoundedRectangle(rect.toFloat(), 3, 2);
                    g.fillRect(rect.getX(), rect.getY() + 25, 3, rect.getHeight() - 25);
                    g.fillRect(rect.getRight() - 3, rect.getY() + 25, 3, rect.getHeight() - 25);
                }
            }
        }
        drawSelection(selection.range, selection.tracks);
        if (!drag.is_null() && dragged)
        {
            if (drag["mode"] == "selection")
                drawSelection(
                    {{"start_samples", std::min(dragStart, dragEnd)}, {"end_samples", std::max(dragStart, dragEnd)}},
                    rangeTracks());
            else if (drag["mode"] != "fade_in" && drag["mode"] != "fade_out" && drag["mode"] != "loop_start" &&
                     drag["mode"] != "loop_end")
            {
                auto preview = drag["clip"];
                preview["start_samples"] = dragStart;
                preview["length_samples"] = dragEnd - dragStart;
                g.setColour(accent().withAlpha(.25f));
                g.fillRect(clipRect(preview, drag["row"]));
            }
        }
        int x = int(std::round(axis.pixelAt(facts.value("position_samples", int64_t(0)))));
        g.setColour(juce::Colour(0xffedca72));
        g.drawVerticalLine(x, 0, float(getHeight()));
        if (facts.value("tracks", Json::array()).empty())
        {
            g.setColour(juce::Colour(0xffb2c3d4));
            g.setFont(juce::FontOptions(18));
            g.drawText(text("导入音频，开始制作"), timelineLeft() + 20, rulerHeight() + 12,
                       getWidth() - timelineLeft() - 40, 32, juce::Justification::centred);
            g.setFont(juce::FontOptions(13));
            g.drawText(text("⌘I 导入 · 空格播放 / 停止 · ⌘Z 撤销"), timelineLeft() + 20, rulerHeight() + 52,
                       getWidth() - timelineLeft() - 40, 26, juce::Justification::centred);
        }
    }
    bool cancelScrubGesture()
    {
        if (!scrubGesture)
            return false;
        scrubGesture = false;
        if (onScrub)
            onScrub("cancel", Json::object());
        repaint();
        return true;
    }
    bool cancelZoomGesture()
    {
        if (!zoomGesture.active)
            return false;
        zoomGesture.cancel();
        resized();
        repaint();
        return true;
    }
    bool cancelTimeSelection()
    {
        if (drag.is_null() || drag.value("mode", std::string{}) != "selection")
            return false;
        drag = nullptr;
        dragged = false;
        for (auto& lane : automationLanes)
            lane->cancel();
        updateAutomation();
        repaint();
        return true;
    }
    void paintOverChildren(juce::Graphics& g) override
    {
        zoomGesture.paint(g, coordinates(), rulerHeight(), getHeight() - 16);
    }
    void visibilityChanged() override
    {
        if (!isShowing())
        {
            cancelTimeSelection();
            cancelScrubGesture();
            zoomGesture.cancel();
        }
    }
    void resized() override
    {
        if (zoomGesture.active && !zoomGesture.coordinatesMatch(double(timelineLeft()),
                                                                double(std::max(1, getWidth() - timelineLeft() - 16))))
            zoomGesture.cancel();
        if (scrubGesture && (scrubLeft != timelineLeft() || scrubWidth != getWidth()))
            cancelScrubGesture();
        if (!drag.is_null() && drag.value("mode", std::string{}) == "selection" &&
            (drag.value("canvas_left", timelineLeft()) != timelineLeft() ||
             drag.value("canvas_width", getWidth()) != getWidth()))
            cancelTimeSelection();
        rowOffsets.clear();
        rowOffsets.push_back(0);
        for (int i = 0; i < visibleRows(); ++i)
            rowOffsets.push_back(rowOffsets.back() + rowHeight(i));
        rulerSelector.setBounds(3, 1, 40, 25);
        horizontal.setBounds(timelineLeft(), getHeight() - 14, std::max(1, getWidth() - timelineLeft() - 16), 14);
        waveformControls.setBounds(getWidth() - 14, rulerHeight(), 14, 54);
        vertical.setBounds(getWidth() - 14, rulerHeight() + 54, 14, std::max(1, getHeight() - rulerHeight() - 68));
        const auto axis = coordinates();
        horizontal.setRangeLimits(
            0, double(std::max(facts.value("length_samples", int64_t(0)) + axis.span, axis.start + axis.span)),
            juce::dontSendNotification);
        horizontal.setCurrentRange(double(axis.start), double(axis.span), juce::dontSendNotification);
        horizontal.setSingleStepSize(double(axis.span) / 10);
        vertical.setRangeLimits(0, std::max(1, visibleRows()), juce::dontSendNotification);
        const int first = std::clamp(view.value("first_row", 0), 0, std::max(0, visibleRows() - 1));
        int page = 0, used = 0;
        while (first + page < visibleRows() && used < getHeight() - rulerHeight() - 14)
            used += rowHeight(first + page++);
        vertical.setCurrentRange(first, std::max(1, page), juce::dontSendNotification);
        vertical.setSingleStepSize(1);

        for (size_t i = 0; i < controls.size(); ++i)
        {
            controls[i]->setBounds(0, rowY(int(i)), 242, rowHeight(int(i)) - 1);
            controls[i]->setVisible(rowY(int(i)) >= rulerHeight() && rowY(int(i)) < getHeight() - 16);
            columns[i]->setBounds(250, rowY(int(i)), timelineLeft() - 250, rowHeight(int(i)) - 1);
            columns[i]->configure(view.value("edit_views", Json::object()), columnWidth());
            columns[i]->setVisible(columnCount() > 0 && controls[i]->isVisible());
            auto bounds = juce::Rectangle<int>(timelineLeft(), rowY(int(i)), getWidth() - timelineLeft() - 16,
                                               rowHeight(int(i)) - 1);
            automationLanes[i]->setBounds(bounds);
            automationLanes[i]->setVisible(!viewParameter(trackIDs[i]).empty() && controls[i]->isVisible());
        }
        for (size_t i = 0; i < facts.value("tracks", Json::array()).size(); ++i)
            for (const auto& c : facts["tracks"][i]["clips"])
                if (headers.contains(c["id"]))
                {
                    auto r = clipRect(c, int(i)).reduced(3).removeFromTop(20).getIntersection(
                        juce::Rectangle<int>(timelineLeft(), rulerHeight(), getWidth() - timelineLeft() - 16,
                                             getHeight() - rulerHeight() - 16));
                    headers.at(c["id"])->setBounds(r);
                    headers.at(c["id"])->setVisible(!r.isEmpty() && viewParameter(facts["tracks"][i]["id"]).empty());
                }
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        juce::MouseCursor cursor(juce::MouseCursor::NormalCursor);
        const int row = rowAt(e.y);
        if (e.x >= timelineLeft() && e.x < getWidth() - 14 && row >= 0 && row < int(trackIDs.size()))
        {
            const auto axis = coordinates();
            for (const auto& clip : facts["tracks"][size_t(row)]["clips"])
                if (clipRect(clip, row).contains(e.getPosition()))
                {
                    const bool audio = clip.value("kind", std::string{}) == "audio" &&
                                       clip.value("editable_audio", false) && !clip.value("locked", false);
                    const auto gesture =
                        editing.gesture(e.x, e.y, clipRect(clip, row), audio, clip.value("fade_in_samples", int64_t(0)),
                                        clip.value("fade_out_samples", int64_t(0)), axis.width / axis.span);
                    // Locking forbids edits, not audition. Use the same audio
                    // hot zones as mouseDown even when edit handles are disabled.
                    const auto scrubRegion =
                        editing.gesture(e.x, e.y, clipRect(clip, row), true, clip.value("fade_in_samples", int64_t(0)),
                                        clip.value("fade_out_samples", int64_t(0)), axis.width / axis.span);
                    if (clip["kind"] == "audio" && viewParameter(trackIDs[size_t(row)]).empty() &&
                        ScrubGesture::accepts(editing.tool, scrubRegion,
                                              e.mods.withFlags(juce::ModifierKeys::leftButtonModifier)))
                        cursor = juce::MouseCursor::CrosshairCursor;
                    else if (gesture == EditingModel::Gesture::select)
                        cursor = juce::MouseCursor::IBeamCursor;
                    else if (gesture == EditingModel::Gesture::move && audio)
                        cursor = juce::MouseCursor::DraggingHandCursor;
                    else if (audio &&
                             (gesture == EditingModel::Gesture::left || gesture == EditingModel::Gesture::right ||
                              gesture == EditingModel::Gesture::fadeIn || gesture == EditingModel::Gesture::fadeOut))
                        cursor = juce::MouseCursor::LeftRightResizeCursor;
                    break;
                }
        }
        if (ZoomGesture::isTool(editing.tool) && e.x >= timelineLeft())
            cursor = juce::MouseCursor::CrosshairCursor;
        setMouseCursor(cursor);
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        drag = nullptr;
        dragged = false;
        if (e.x >= 44 && e.x < timelineLeft() && e.y < rulerHeight())
        {
            if (const auto* ruler = Rulers::at(view, e.y); ruler && onRulerCommand)
            {
                if (e.mods.isAltDown())
                    onRulerCommand(ruler->command);
                else if (ruler->mainCommand != 0)
                    onRulerCommand(ruler->mainCommand);
            }
            return;
        }
        if (e.x < timelineLeft() || e.x >= getWidth() - 14 || e.y >= getHeight() - 14)
            return;
        const auto axis = coordinates();
        const int row = std::clamp(rowAt(e.y), 0, std::max(0, visibleRows() - 1));
        // macOS Ctrl-left-click is also a popup gesture; the documented Cmd+Ctrl ruler chord wins.
        const bool rulerZoom =
            e.y < rulerHeight() && e.mods.isCommandDown() && e.mods.isCtrlDown() && e.mods.isLeftButtonDown();
        const bool continuousZoom =
            e.mods.isCtrlDown() && e.mods.isLeftButtonDown() && ZoomGesture::isTool(editing.tool);
        if (rulerZoom || ((!e.mods.isPopupMenu() || continuousZoom) && ZoomGesture::isTool(editing.tool) &&
                          e.y >= rulerHeight() && rowAt(e.y) >= 0 && rowAt(e.y) < visibleRows()))
        {
            juce::Rectangle<int> channel;
            if (e.y >= rulerHeight() && e.mods.isCommandDown() && !e.mods.isCtrlDown())
                for (const auto& clip : facts["tracks"][row]["clips"])
                    if (clip["kind"] == "audio" && clipRect(clip, row).contains(e.getPosition()))
                    {
                        const auto area = clipRect(clip, row).reduced(0, 25).getIntersection(
                            juce::Rectangle<int>(timelineLeft(), rulerHeight(), getWidth() - timelineLeft() - 16,
                                                 getHeight() - rulerHeight() - 16));
                        channel = waves.channelAt(clip, area, e.y);
                        break;
                    }
            zoomGesture.begin(e, axis, facts, view, e.y >= rulerHeight() ? trackIDs[size_t(row)] : std::string{},
                              !ZoomGesture::isTool(editing.tool), channel);
            repaint();
            return;
        }
        // The small gap at an adjacent audio lane boundary auditions both
        // actual sources; track headers and rulers retain their own gestures.
        if (editing.tool == "scrubber" && ScrubGesture::accepts(editing.tool, EditingModel::Gesture::move, e.mods) &&
            e.y >= rulerHeight() && rowAt(e.y) >= 0 && rowAt(e.y) < visibleRows())
        {
            int upper = -1;
            if (row > 0 && std::abs(e.y - rowY(row)) <= 6)
                upper = row - 1;
            else if (row + 1 < visibleRows() && std::abs(e.y - rowY(row + 1)) <= 6)
                upper = row;
            if (upper >= 0 && facts["tracks"][upper]["type"] == "audio" &&
                facts["tracks"][upper + 1]["type"] == "audio" && viewParameter(trackIDs[size_t(upper)]).empty() &&
                viewParameter(trackIDs[size_t(upper + 1)]).empty())
            {
                const auto sample = axis.sampleAt(e.x);
                for (const int owner : {upper, upper + 1})
                    for (const auto& clip : facts["tracks"][owner]["clips"])
                        if (clip["kind"] == "audio" && sample >= clip["start_samples"].get<int64_t>() &&
                            sample < clip["start_samples"].get<int64_t>() + clip["length_samples"].get<int64_t>())
                        {
                            beginScrubPointer(clip, e, owner,
                                              Json::array({trackIDs[size_t(upper)], trackIDs[size_t(upper + 1)]}));
                            return;
                        }
                return;
            }
        }
        // Ctrl-left-click wins over macOS popup recognition only in a real
        // Selector region. Smart trim/fade/grab regions keep their own behavior.
        if (e.y >= rulerHeight() && rowAt(e.y) >= 0 && rowAt(e.y) < visibleRows() &&
            viewParameter(trackIDs[size_t(row)]).empty())
        {
            for (const auto& clip : facts["tracks"][row]["clips"])
                if (clip["kind"] == "audio" && clipRect(clip, row).contains(e.getPosition()))
                {
                    const auto region =
                        editing.gesture(e.x, e.y, clipRect(clip, row), true, clip.value("fade_in_samples", int64_t(0)),
                                        clip.value("fade_out_samples", int64_t(0)), axis.width / axis.span);
                    if (ScrubGesture::accepts(editing.tool, region, e.mods))
                    {
                        beginScrubPointer(clip, e, row);
                        return;
                    }
                }
        }
        // A dedicated/temporary Scrubber press in empty space cannot seek,
        // select, edit or fall through to a Ctrl popup. No source means no sound.
        if (e.y >= rulerHeight() && ScrubGesture::accepts(editing.tool, EditingModel::Gesture::move, e.mods))
            return;
        const auto point = snapped(axis.sampleAt(e.x), e.mods);
        if (e.y >= rulerHeight() &&
            (editing.tool == "pencil" || (editing.tool != "selector" && row < int(trackIDs.size()) &&
                                          !viewParameter(trackIDs[size_t(row)]).empty())))
            return;
        if (e.mods.isPopupMenu())
        {
            std::string clip;
            if (e.y >= rulerHeight() && row < int(trackIDs.size()))
                for (const auto& c : facts["tracks"][row]["clips"])
                    if (clipRect(c, row).contains(e.getPosition()))
                        clip = c["id"];
            if (onContext)
                onContext(clip);
            return;
        }
        if (!facts.value("playing", false))
            for (const bool start : {true, false})
                if (loopHandleRect(start).contains(e.getPosition()))
                {
                    const auto loop = facts["transport_settings"]["loop_range"];
                    drag = {{"mode", start ? "loop_start" : "loop_end"},
                            {"revision", facts["revision"]},
                            {"session", facts["session_token"]},
                            {"loop", loop}};
                    dragX = e.x;
                    dragScale = axis.span / axis.width;
                    dragPoint = loop[start ? "start_samples" : "end_samples"].get<int64_t>();
                    dragStart = loop["start_samples"];
                    dragEnd = loop["end_samples"];
                    return;
                }
        const auto* ruler = Rulers::at(view, e.y);
        if (ruler && (std::string(ruler->key) == "tempo" || std::string(ruler->key) == "meter"))
        {
            seek(point);
            return;
        }
        if (markerLaneY() >= 0 && e.y >= markerLaneY() && e.y < markerLaneY() + 20)
        {
            Json nearest = nullptr;
            double distance = 10.0;
            for (const auto& marker : facts.value("markers", Json::array()))
            {
                const auto delta = std::abs(axis.pixelAt(marker["position_samples"].get<int64_t>()) - double(e.x));
                if (delta <= distance)
                {
                    nearest = marker;
                    distance = delta;
                }
            }
            if (!nearest.is_null())
            {
                const auto id = nearest.at("id").get<std::string>();
                const auto position = nearest.at("position_samples").get<int64_t>();
                seek(position);
                if (onMarkerClick)
                    onMarkerClick(id);
            }
            else
                seek(point);
            return;
        }
        dragX = e.x;
        dragScale = axis.span / axis.width;
        dragPoint = axis.sampleAt(e.x);
        dragRow = row;
        endRow = row;
        if (e.y < rulerHeight() || editing.tool == "selector")
        {
            if (!facts.value("playing", false))
                beginTimeSelection(e, point);
            else
                seek(point);
            return;
        }
        if (e.y >= rulerHeight() && row < int(trackIDs.size()))
        {
            for (const auto c : facts["tracks"][row]["clips"])
                if (clipRect(c, row).contains(e.getPosition()))
                {
                    const auto clipBounds = clipRect(c, row);
                    const auto clipStart = c["start_samples"].get<int64_t>();
                    const auto clipEnd = clipStart + c["length_samples"].get<int64_t>();
                    const bool audio =
                        c["kind"] == "audio" && c.value("editable_audio", false) && !c.value("locked", false);
                    const auto toolGesture =
                        editing.gesture(e.x, e.y, clipBounds, audio, c.value("fade_in_samples", int64_t(0)),
                                        c.value("fade_out_samples", int64_t(0)), axis.width / axis.span);
                    if (toolGesture == EditingModel::Gesture::select && editing.tool == "smart")
                    {
                        if (!facts.value("playing", false))
                            beginTimeSelection(e, point);
                        else
                            seek(point);
                        return;
                    }
                    if (onClipSelection)
                        onClipSelection(c["id"], e.mods.isShiftDown());
                    else
                    {
                        select(trackIDs[row]);
                        if (selectClip)
                            selectClip(c["id"]);
                    }
                    if (audio && !facts.value("playing", false))
                    {
                        const auto gesture = toolGesture;
                        const auto mode = gesture == EditingModel::Gesture::left      ? "left"
                                          : gesture == EditingModel::Gesture::right   ? "right"
                                          : gesture == EditingModel::Gesture::fadeIn  ? "fade_in"
                                          : gesture == EditingModel::Gesture::fadeOut ? "fade_out"
                                                                                      : "move";
                        drag = {{"clip", c},
                                {"clip_id", c["id"]},
                                {"revision", facts["revision"]},
                                {"session", facts["session_token"]},
                                {"row", row},
                                {"mode", mode},
                                {"preview_fade_in", c.value("fade_in_samples", int64_t(0))},
                                {"preview_fade_out", c.value("fade_out_samples", int64_t(0))}};
                        dragStart = clipStart;
                        dragEnd = clipEnd;
                        if (gesture != EditingModel::Gesture::move)
                            if (gesture == EditingModel::Gesture::left || gesture == EditingModel::Gesture::right)
                                mouseDrag(e);
                    }
                    seek(point);
                    return;
                }
            if (editing.tool == "smart")
            {
                if (!facts.value("playing", false))
                    beginTimeSelection(e, point);
                else
                    seek(point);
                return;
            }
            select(trackIDs[row]);
        }
        seek(point);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (scrubGesture)
        {
            const auto request = scrubMotion.move(e, juce::Time::getMillisecondCounterHiRes());
            if (!onScrub || !onScrub("speed", request))
                cancelScrubGesture();
            return;
        }
        if (zoomGesture.active)
        {
            zoomGesture.move(e);
            resized();
            repaint();
            return;
        }
        if (drag.is_null())
            return;
        const auto maximum = std::llround(te::Edit::maximumLength * 48000);
        const auto raw = std::clamp(dragPoint + std::llround((e.x - dragX) * dragScale), int64_t(0), maximum);
        std::string mode = drag["mode"];
        if (mode == "selection")
        {
            dragEnd = snapped(raw, e.mods);
            endRow = std::clamp(rowAt(e.y), 0, std::max(0, visibleRows() - 1));
            dragged = drag.value("extend", false) || std::abs(e.x - dragX) >= 3 || endRow != dragRow;
            repaint();
            return;
        }
        if (mode == "loop_start" || mode == "loop_end")
        {
            if (mode == "loop_start")
                dragStart = std::clamp(snapped(raw, e.mods), int64_t(0), dragEnd - 1);
            else
                dragEnd = std::clamp(snapped(raw, e.mods), dragStart + 1, maximum);
            dragged = std::abs(e.x - dragX) >= 3;
            repaint();
            return;
        }
        const auto& c = drag["clip"];
        if (mode == "fade_in" || mode == "fade_out")
        {
            const auto point = snapped(raw, e.mods);
            const auto fadeIn = c.value("fade_in_samples", int64_t(0));
            const auto fadeOut = c.value("fade_out_samples", int64_t(0));
            if (mode == "fade_in")
                drag["preview_fade_in"] = std::clamp(point - c["start_samples"].get<int64_t>(), int64_t(0),
                                                     c["length_samples"].get<int64_t>() - fadeOut);
            else
                drag["preview_fade_out"] =
                    std::clamp(c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>() - point,
                               int64_t(0), c["length_samples"].get<int64_t>() - fadeIn);
            dragged = std::abs(e.x - dragX) >= 3;
            repaint();
            return;
        }
        int64_t start = c["start_samples"], length = c["length_samples"], offset = c["source_offset_samples"],
                source =
                    std::llround(c["source_frames"].get<int64_t>() / c["source_sample_rate"].get<double>() * 48000);
        if (mode == "move")
        {
            dragStart = std::clamp(snapped(std::max(int64_t(0), start + raw - dragPoint), e.mods), int64_t(0),
                                   maximum - length);
            dragEnd = dragStart + length;
        }
        else if (mode == "left")
        {
            dragStart = std::clamp(snapped(raw, e.mods), std::max(int64_t(0), start - offset), start + length - 1);
            dragEnd = start + length;
        }
        else
        {
            dragStart = start;
            dragEnd = std::clamp(snapped(raw, e.mods), start + 1, start + source - offset);
        }
        dragged = mode != "move" || std::abs(e.x - dragX) >= 3;
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (scrubGesture)
        {
            scrubGesture = false;
            if (onScrub)
                onScrub("end", Json::object());
            return;
        }
        if (zoomGesture.active)
        {
            const auto request = zoomGesture.finish();
            resized();
            repaint();
            if (onZoomGesture)
                onZoomGesture(request, zoomGesture.session, zoomGesture.revision);
            return;
        }
        if (drag.is_null())
            return;
        auto captured = drag;
        const auto owners = rangeTracks();
        drag = nullptr;
        repaint();
        if (captured["session"] != facts["session_token"])
            return;
        std::string mode = captured["mode"];
        if (mode == "selection")
        {
            if (onRange)
                onRange(dragged && dragStart != dragEnd ? Json{{"start_samples", std::min(dragStart, dragEnd)},
                                                               {"end_samples", std::max(dragStart, dragEnd)}}
                                                        : Json(nullptr),
                        owners, captured["revision"], captured["session"],
                        dragged ? std::min(dragStart, dragEnd) : dragStart);
            return;
        }
        if (mode == "loop_start" || mode == "loop_end")
        {
            if (dragged && onLoopRange &&
                (dragStart != captured["loop"]["start_samples"] || dragEnd != captured["loop"]["end_samples"]))
                onLoopRange({{"start_samples", dragStart}, {"end_samples", dragEnd}}, captured["revision"]);
            return;
        }
        if (!dragged || !clipWrite)
            return;
        const auto& original = captured["clip"];
        if (mode == "fade_in" || mode == "fade_out")
        {
            const auto fadeIn = captured.value("preview_fade_in", original.value("fade_in_samples", int64_t(0)));
            const auto fadeOut = captured.value("preview_fade_out", original.value("fade_out_samples", int64_t(0)));
            if (fadeIn != original.value("fade_in_samples", int64_t(0)) ||
                fadeOut != original.value("fade_out_samples", int64_t(0)))
                clipWrite("clip.fade",
                          {{"clip", original["id"]},
                           {"in_samples", fadeIn},
                           {"out_samples", fadeOut},
                           {"in_curve", original.value("fade_in_curve", std::string("linear"))},
                           {"out_curve", original.value("fade_out_curve", std::string("linear"))}},
                          captured["revision"]);
            return;
        }
        if (dragStart == original["start_samples"] &&
            dragEnd == original["start_samples"].get<int64_t>() + original["length_samples"].get<int64_t>())
            return;
        if (mode == "move")
            clipWrite("clip.move", {{"clip", original["id"]}, {"position_samples", dragStart}}, captured["revision"]);
        else
            clipWrite("clip.trim", {{"clip", original["id"]}, {"start_samples", dragStart}, {"end_samples", dragEnd}},
                      captured["revision"]);
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (!onViewChange || (wheel.deltaX == 0 && wheel.deltaY == 0))
            return;
        auto axis = coordinates();
        if (e.mods.isCommandDown())
        {
            auto next = std::clamp(int64_t(std::llround(axis.span * (wheel.deltaY > 0 ? .8 : 1.25))), int64_t(480),
                                   std::llround(te::Edit::maximumLength * 48000));
            auto anchor = axis.sampleAt(e.x);
            auto first = std::llround(anchor - (anchor - axis.start) * double(next) / axis.span);
            onViewChange({{"span_samples", next},
                          {"start_samples",
                           std::clamp(first, int64_t(0), std::llround(te::Edit::maximumLength * 48000) - next)}});
        }
        else if (e.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
        {
            auto delta = std::llround((wheel.deltaX != 0 ? wheel.deltaX : wheel.deltaY) * axis.span * .3);
            onViewChange({{"start_samples", std::clamp(axis.start - delta, int64_t(0),
                                                       std::llround(te::Edit::maximumLength * 48000) - axis.span)}});
        }
        else
        {
            const int first =
                std::clamp(view.value("first_row", 0) + (wheel.deltaY > 0 ? -1 : 1), 0, std::max(0, visibleRows() - 1));
            onViewChange({{"first_row", first}});
        }
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.x < timelineLeft() || e.y < rulerHeight())
            return;
        const int row = rowAt(e.y);
        if (row >= int(trackIDs.size()))
            return;
        for (const auto& c : facts["tracks"][row]["clips"])
            if (c["kind"] == "midi" && clipRect(c, row).contains(e.getPosition()))
            {
                openMidi(c["id"]);
                return;
            }
    }

private:
    void beginTimeSelection(const juce::MouseEvent& e, int64_t point)
    {
        const bool extend = e.mods.isShiftDown();
        drag = {{"mode", "selection"},
                {"revision", facts["revision"]},
                {"session", facts["session_token"]},
                {"all_tracks", e.y < rulerHeight()},
                {"extend", extend},
                {"owners", extend ? selection.tracks : Json::array()},
                {"canvas_left", timelineLeft()},
                {"canvas_width", getWidth()}};
        dragStart = TimeSelectionGesture::anchor(facts.value("time_selection", Json(nullptr)),
                                                 facts.value("position_samples", int64_t(0)), point, extend);
        dragEnd = point;
        dragged = extend && dragStart != dragEnd;
        repaint();
    }
    std::string viewParameter(const std::string& id) const
    {
        return view.value("track_views", Json::object()).value(id, std::string{});
    }
    void updateAutomation()
    {
        if (!onAutomationQuery || !onAutomationSamples)
            return;
        for (size_t i = 0; i < controls.size(); ++i)
        {
            const auto parameter = viewParameter(trackIDs[i]);
            // Header enumeration only when visible; the Workspace caches actual facts per revision.
            if (!controls[i]->isVisible())
            {
                automationLanes[i]->cancel();
                continue;
            }
            const auto q = onAutomationQuery(trackIDs[i]);
            Json options = Json::array(), selectedLane = nullptr;
            for (const auto& lane : q["lanes"])
            {
                options.push_back({{"id", lane["id"]}, {"name", lane["name"]}});
                if (lane["id"] == parameter)
                    selectedLane = lane;
            }
            controls[i]->configureViews(options, parameter);
            if (parameter.empty())
            {
                automationLanes[i]->cancel();
                continue;
            }
            auto axis = coordinates();
            const auto key = facts["session_token"].get<std::string>() + ":" + facts["revision"].dump() + ":" +
                             parameter + ":" + std::to_string(axis.start) + ":" + std::to_string(axis.span);
            auto& cached = sampled[trackIDs[i]];
            if (cached.first != key)
            {
                cached.first = key;
                cached.second = selectedLane.is_null()
                                    ? Json::array()
                                    : onAutomationSamples(trackIDs[i], parameter, axis.start, axis.start + axis.span);
            }
            automationLanes[i]->setTimeline(grid, facts["position_samples"]);
            automationLanes[i]->update(trackIDs[i], selectedLane, cached.second, axis, editing, selection,
                                       facts["revision"], facts["session_token"],
                                       !facts.value("playing", false) && !facts.value("recording", false) &&
                                           facts.value("parameter_capture", Json(nullptr)).is_null());
        }
    }
    std::map<std::string, std::pair<std::string, Json>> sampled;
    std::vector<std::unique_ptr<AutomationLane>> automationLanes;
    void beginScrubPointer(const Json& clip, const juce::MouseEvent& event, int row, Json tracks = Json::array())
    {
        if (!onScrub)
            return;
        const auto axis = coordinates();
        const auto sample = axis.sampleAt(event.x);
        if (tracks.empty())
        {
            tracks.push_back(trackIDs[size_t(row)]);
            const auto range = facts.value("time_selection", Json(nullptr));
            if (!range.is_null() && sample >= range["start_samples"].get<int64_t>() &&
                sample < range["end_samples"].get<int64_t>() &&
                std::find(selection.tracks.begin(), selection.tracks.end(), trackIDs[size_t(row)]) !=
                    selection.tracks.end())
            {
                Json selectedAudio = Json::array();
                for (const auto& t : facts["tracks"])
                    if (t["type"] == "audio" &&
                        std::find(selection.tracks.begin(), selection.tracks.end(), t["id"]) != selection.tracks.end())
                    {
                        selectedAudio.push_back(t["id"]);
                        if (selectedAudio.size() == 2)
                            break;
                    }
                if (selectedAudio.size() > 1)
                    tracks = selectedAudio;
            }
        }
        scrubSession = facts["session_token"];
        scrubRevision = facts["revision"];
        Json request = {
            {"clip", clip["id"]}, {"position_samples", sample}, {"session", scrubSession}, {"revision", scrubRevision}};
        if (tracks.size() > 1)
            request["tracks"] = tracks;
        if (event.mods.isShiftDown())
            request["extend_selection"] = true;
        scrubGesture = onScrub("begin", request);
        scrubPreparing = scrubGesture;
        scrubBuffering = false;
        scrubMotion.begin(event, axis, juce::Time::getMillisecondCounterHiRes());
        scrubLeft = timelineLeft();
        scrubWidth = getWidth();
    }
    int rowAt(int y) const
    {
        if (rowOffsets.size() < 2)
            return 0;
        const int first = std::clamp(view.value("first_row", 0), 0, std::max(0, visibleRows() - 1));
        const int offset = rowOffsets[size_t(first)] + y - rulerHeight();
        return int(std::upper_bound(rowOffsets.begin(), rowOffsets.end(), offset) - rowOffsets.begin()) - 1;
    }
    int64_t snapped(int64_t position, juce::ModifierKeys modifiers) const
    {
        return editing.mode == "grid" && !modifiers.isCommandDown() && onSnap ? onSnap(position, editing.gridBeats)
                                                                              : position;
    }
    Json rangeTracks() const
    {
        Json owners = drag.is_null() ? Json::array() : drag.value("owners", Json::array());
        if (drag.is_null())
            return owners;
        for (int row = 0; row < visibleRows(); ++row)
            if (drag.value("all_tracks", false) ||
                (row >= std::min(dragRow, endRow) && row <= std::max(dragRow, endRow)))
                if (std::find(owners.begin(), owners.end(), trackIDs[size_t(row)]) == owners.end())
                    owners.push_back(trackIDs[size_t(row)]);
        return owners;
    }
    void scrollBarMoved(juce::ScrollBar* bar, double position) override
    {
        if (!onViewChange)
            return;
        if (bar == &horizontal)
            onViewChange(
                {{"start_samples", std::clamp(std::llround(position), int64_t(0),
                                              std::llround(te::Edit::maximumLength * 48000) - coordinates().span)}});
        else
            onViewChange({{"first_row", std::clamp(int(std::llround(position)), 0, std::max(0, visibleRows() - 1))}});
    }
    void cancelHeightPreview()
    {
        resizeTrack.clear();
        for (auto& c : controls)
            c->cancelHeightGesture();
    }
    std::vector<int> rowOffsets;
    std::string resizeTrack, resizeSession;
    int resizeHeight = 144;
    Json rulerContext = Json::object();
    juce::TextButton rulerSelector;
    juce::ScrollBar horizontal{false}, vertical{true};
    static void drawFade(juce::Graphics& g, juce::Rectangle<int> r, double fraction, const std::string& type, bool in)
    {
        if (fraction <= 0)
            return;
        juce::Path p;
        for (int i = 0; i <= 32; ++i)
        {
            double a = i / 32., theta = a * juce::MathConstants<double>::halfPi,
                   gain = type == "convex"    ? std::sin(theta)
                          : type == "concave" ? 1 - std::cos(theta)
                          : type == "s_curve" ? (1 - a) * (1 - std::cos(theta)) + a * std::sin(theta)
                                              : a;
            float x = float(in ? r.getX() + a * fraction * r.getWidth() : r.getRight() - a * fraction * r.getWidth()),
                  y = float(r.getBottom() - 4 - gain * (r.getHeight() - 29));
            if (i == 0)
                p.startNewSubPath(x, y);
            else
                p.lineTo(x, y);
        }
        g.strokePath(p, juce::PathStrokeType(1));
    }
    int64_t sampleAt(int x) const
    {
        return coordinates().sampleAt(x);
    }
    Writer write;
    std::function<void(std::string)> select;
    std::function<void(int64_t)> seek;
    Waveforms& waves;
    std::function<void(std::string)> openMidi, selectClip;
    std::function<void(const std::string&, Json, uint64_t)> clipWrite;
    EditingModel editing;
    SelectionModel selection;
    bool scrubGesture = false, scrubPreparing = false, scrubBuffering = false;
    std::string scrubSession;
    uint64_t scrubRevision = 0;
    ScrubGesture scrubMotion;
    int scrubLeft = 0, scrubWidth = 0;
    ZoomGesture zoomGesture;
    WaveformZoomControls waveformControls;
    Json view = Json::object();
    Json facts = Json::object(), grid = Json::array(), drag = nullptr;
    std::string selected, selectedClip;
    int dragX = 0, dragRow = 0, endRow = 0;
    int64_t dragStart = 0, dragEnd = 0, dragPoint = 0;
    double dragScale = 0;
    bool dragged = false;
    std::vector<std::string> trackIDs;
    std::vector<std::unique_ptr<TrackHeader>> controls;
    std::vector<std::unique_ptr<EditWindowViews>> columns;
    std::map<std::string, std::unique_ptr<juce::TextButton>> headers;
};

} // namespace ndaw::desktop
