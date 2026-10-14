// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Waveforms.h"
#include "PanelTextEditor.h"
namespace ndaw::desktop
{
class PlaylistPanel final : public juce::Component
{
public:
    using Audition = std::function<Json(const std::string&, const Json&)>;
    using Submit = std::function<Json(const std::string&, Json, uint64_t, const std::string&)>;
    PlaylistPanel(Submit submit, std::function<void()> close, std::function<Json(const std::string&)> query,
                  Audition audition)
        : submit(std::move(submit)), close(std::move(close)), query(std::move(query)), audition(std::move(audition)),
          canvas(*this), draftModel(*this)
    {
        setComponentID("playlist.panel");
        addAndMakeVisible(title);
        addAndMakeVisible(name);
        addAndMakeVisible(status);
        addAndMakeVisible(view);
        addAndMakeVisible(scroll);
        addAndMakeVisible(zoom);
        addAndMakeVisible(trackChoice);
        trackChoice.setComponentID("playlist.source.track");
        trackChoice.setTooltip(text("切换来源轨道；Shift 点波形沿用共同选区，再加入候选"));
        trackChoice.onChange = [this]
        {
            const int index = trackChoice.getSelectedId() - 1;
            if (data.is_null() || index < 0 || index >= int(data["tracks"].size()))
                return;
            try
            {
                if (auditionStatus.value("active", false))
                    updateAudition(this->audition("stop", Json::object()));
                const std::string next = data["tracks"][index]["id"];
                auto state = this->query(next);
                track = next;
                source.clear();
                sourceClip.clear();
                bind(std::move(state));
                updateName();
            }
            catch (const std::exception& error)
            {
                status.setText(text(error.what()), juce::dontSendNotification);
            }
        };
        addAndMakeVisible(candidateName);
        addAndMakeVisible(draftList);
        candidateName.setComponentID("playlist.candidate.name");
        candidateName.setText("Candidate Comp");
        candidateName.setTooltip(text("候选 Comp 名称；创建后不自动切换播放或目标"));
        candidateName.setInputRestrictions(64);
        draftList.setComponentID("playlist.candidate.segments");
        draftList.setModel(&draftModel);
        draftList.setRowHeight(24);
        name.setComponentID("playlist.name");
        name.setInputRestrictions(64);
        title.setFont(juce::FontOptions(20, juce::Font::bold));
        view.setViewedComponent(&canvas, false);
        view.setScrollBarsShown(true, false);
        canvas.setComponentID("playlist.lanes");
        zoom.setRange(1., 64., .25);
        zoom.setValue(1., juce::dontSendNotification);
        zoom.setSliderStyle(juce::Slider::LinearHorizontal);
        zoom.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        zoom.setTooltip(text("Playlist 水平缩放；下方滚动条移动时间窗口"));
        zoom.onValueChange = [this] { layoutTime(); };
        scroll.addListener(&canvas);
        for (auto [id, label] :
             std::vector<std::pair<int, const char*>>{{515, "新建"},           {516, "复制"},
                                                      {517, "重命名"},         {518, "设为目标"},
                                                      {519, "提升选区 ↑"},     {520, "新建 Comp"},
                                                      {521, "登记 Take"},      {522, "锁定 / 解锁"},
                                                      {523, "用于播放"},       {524, "关闭"},
                                                      {525, "加入候选"},       {526, "移除所选段"},
                                                      {527, "创建候选"},       {528, "清空候选"},
                                                      {529, "恢复整组原播放"}, {530, "解除 Comp 关联"},
                                                      {531, "下一来源轨"},     {532, "试听来源选区"},
                                                      {533, "试听候选选段"},   {534, "上一段"},
                                                      {535, "下一段"},         {536, "停止试听 / 返回"},
                                                      {537, "试听所选列表"}})
        {
            auto button = std::make_unique<juce::TextButton>(text(label));
            addAndMakeVisible(*button);
            buttons[id] = std::move(button);
        }
        status.setComponentID("playlist.status");
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        name.connect(manager);
        candidateName.connect(manager, 527);
        for (auto& [id, b] : buttons)
        {
            b->setComponentID("ui.command:" + juce::String(id));
            b->setCommandToTrigger(&manager, id, true);
        }
    }
    void bind(Json next)
    {
        const bool changed =
            data.is_null() || data["revision"] != next["revision"] || data["session_token"] != next["session_token"];
        if (changed)
        {
            if (!draft.empty())
                draftNotice = text("工程版本已变化，未提交的候选清单已清空；请重新选段。");
            draft = Json::array();
            draftLabels.clear();
            draftList.updateContent();
            sourceClip.clear();
            begin = end = 0;
            canvas.dragging = false;
        }
        data = std::move(next);
        trackChoice.clear(juce::dontSendNotification);
        int index = 0;
        for (const auto& t : data["tracks"])
        {
            trackChoice.addItem(text(t["name"].get<std::string>()), ++index);
            if (t["id"] == track)
                trackChoice.setSelectedId(index, juce::dontSendNotification);
        }
        bool found = false;
        for (const auto& list : data["items"])
            if (list["id"] == source)
                found = true;
        if (!found)
            source = data["active"];
        if (changed)
            updateName();
        title.setText(text(data["track_name"].get<std::string>() + " · Playlists / Comp"), juce::dontSendNotification);
        Json tracks = Json::array();
        for (const auto& list : data["items"])
            tracks.push_back({{"clips", list["clips"]}});
        canvas.waves.update({{"tracks", tracks}});
        duration = 48000 * 10;
        for (const auto& list : data["items"])
            for (const auto& c : list["clips"])
                duration = std::max(duration, c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>());
        refreshStatus();
        layoutTime();
        resized();
        canvas.repaint();
    }
    const std::string& trackID() const
    {
        return track;
    }
    void updateAudition(Json next)
    {
        auditionStatus = std::move(next);
        refreshStatus();
    }
    int playbackCommand() const
    {
        auto* focus = juce::Component::getCurrentlyFocusedComponent();
        if (focus && (focus == &draftList || draftList.isParentOf(focus)) && canExecute(533))
            return 533;
        return canExecute(532) ? 532 : 537;
    }
    void setTrack(std::string id)
    {
        track = std::move(id);
        source.clear();
        data = nullptr;
        draft = Json::array();
        draftLabels.clear();
        draftNotice.clear();
    }
    bool canExecute(int id) const
    {
        if (id == 524)
            return true;
        if (id == 536)
            return auditionStatus.value("active", false);
        if (data.is_null() || source.empty())
            return false;
        if (id == 532)
            return !sourceClip.empty() && end > begin;
        if (id == 533)
            return draftList.getSelectedRow() >= 0 && draftList.getSelectedRow() < int(draft.size());
        if (id == 534 || id == 535)
            return !draft.empty();
        if (id == 537)
            return !selectedList()["clips"].empty();
        if (auditionStatus.value("active", false))
            return false;
        if (id == 529 || id == 530)
            return !selectedList().value("linked_set", std::string{}).empty();
        if (id == 531)
            return data["tracks"].size() > 1;
        if (id == 519)
            return !sourceClip.empty() && end > begin && data["target"] != source;
        if (id == 521)
            return !sourceClip.empty();
        if (id == 525)
            return !sourceClip.empty() && end > begin && draft.size() < 64;
        if (id == 526)
            return draftList.getSelectedRow() >= 0 && draftList.getSelectedRow() < int(draft.size());
        if (id == 527 || id == 528)
            return !draft.empty();
        return true;
    }
    void execute(int id)
    {
        if (id == 524)
        {
            if (auditionStatus.value("active", false))
                audition("stop", Json::object());
            close();
            return;
        }
        if (!canExecute(id))
            return;
        try
        {
            if (id >= 532 && id <= 537)
            {
                if (id == 536)
                {
                    updateAudition(audition("stop", Json::object()));
                    return;
                }
                Json request{
                    {"track", track}, {"base_revision", data["revision"]}, {"session_token", data["session_token"]}};
                if (id == 534 || id == 535)
                {
                    const int count = int(draft.size());
                    int row = draftList.getSelectedRow();
                    row = row < 0 ? 0 : (row + (id == 534 ? count - 1 : 1)) % count;
                    draftList.selectRow(row);
                    id = 533;
                }
                if (id == 533)
                {
                    const auto& selected = draft[size_t(draftList.getSelectedRow())];
                    request["start_samples"] = selected["start_samples"];
                    request["end_samples"] = selected["end_samples"];
                    request["segments"] = draft;
                }
                else
                {
                    request["playlist"] = source;
                    int64_t first = begin, last = end;
                    if (id == 537)
                    {
                        first = std::numeric_limits<int64_t>::max();
                        last = 0;
                        for (const auto& clip : selectedList()["clips"])
                        {
                            first = std::min(first, clip["start_samples"].get<int64_t>());
                            last = std::max(last, clip["start_samples"].get<int64_t>() +
                                                      clip["length_samples"].get<int64_t>());
                        }
                    }
                    request["start_samples"] = first;
                    request["end_samples"] = last;
                }
                updateAudition(audition("start", request));
                return;
            }
            if (id == 531)
            {
                trackChoice.setSelectedId(trackChoice.getSelectedId() % int(data["tracks"].size()) + 1,
                                          juce::sendNotificationSync);
                return;
            }
            if (id == 525)
            {
                for (const auto& segment : draft)
                    if (segment["track"] == track && segment["start_samples"].get<int64_t>() < end &&
                        segment["end_samples"].get<int64_t>() > begin)
                        throw std::runtime_error("候选片段有重叠，请先移除旧选段或缩短选区");
                draft.push_back({{"track", track},
                                 {"source", source},
                                 {"clip", sourceClip},
                                 {"start_samples", begin},
                                 {"end_samples", end}});
                draftLabels[track + ":" + source] =
                    data["track_name"].get<std::string>() + " / " + selectedList()["name"].get<std::string>();
                draftNotice.clear();
                draftList.updateContent();
                draftList.selectRow(int(draft.size()) - 1);
                refreshStatus();
                return;
            }
            if (id == 526 || id == 528)
            {
                if (id == 526)
                    draft.erase(draft.begin() + draftList.getSelectedRow());
                else
                    draft = Json::array();
                std::erase_if(draftLabels,
                              [this](const auto& entry)
                              {
                                  return std::none_of(draft.begin(), draft.end(),
                                                      [&](const Json& s)
                                                      {
                                                          return s["track"].get<std::string>() + ":" +
                                                                     s["source"].get<std::string>() ==
                                                                 entry.first;
                                                      });
                              });
                draftNotice.clear();
                draftList.updateContent();
                refreshStatus();
                return;
            }
            Json a{{"track", track}};
            std::string cmd;
            if (id == 527)
            {
                std::set<std::string> members;
                for (const auto& s : draft)
                    members.insert(s["track"].get<std::string>());
                cmd = members.size() > 1 ? "playlist.comp.linked.create" : "playlist.comp.create";
                a["track"] = draft[0]["track"];
                a["name"] = candidateName.getText().toStdString();
                a["segments"] = draft;
                if (members.size() == 1)
                    for (auto& s : a["segments"])
                        s.erase("track");
            }
            if (id == 529 || id == 530)
            {
                cmd = id == 529 ? "playlist.comp.restore" : "playlist.comp.detach";
                a["playlist"] = source;
            }
            if (id == 515 || id == 516 || id == 520)
            {
                cmd = "playlist.create";
                a["name"] = id == 520 ? "Comp" : id == 515 ? "Playlist" : name.getText().toStdString() + " copy";
                a["role"] = id == 520 ? "comp" : "main";
                if (id != 515)
                    a["source"] = source;
            }
            if (id == 517)
            {
                cmd = "playlist.rename";
                a["playlist"] = source;
                a["name"] = name.getText().toStdString();
            }
            if (id == 518 || id == 523)
            {
                cmd = id == 518 ? "playlist.target" : "playlist.select";
                a["playlist"] = source;
            }
            if (id == 522)
            {
                cmd = "playlist.lock";
                a["playlist"] = source;
                for (const auto& list : data["items"])
                    if (list["id"] == source)
                        a["locked"] = !list["locked"].get<bool>();
            }
            if (id == 519)
            {
                cmd = "playlist.promote";
                a.update({{"source", source},
                          {"target", data["target"]},
                          {"clip", sourceClip},
                          {"start_samples", begin},
                          {"end_samples", end}});
            }
            if (id == 521)
            {
                cmd = "playlist.take.register";
                a.update({{"source", source}, {"clip", sourceClip}, {"name", name.getText().toStdString() + " Take"}});
            }
            const auto receipt = submit(cmd, a, data["revision"], data["session_token"]);
            for (const auto& object : receipt.value("objects", Json::array()))
                if (object["kind"] == "playlist" && object.value("track", track) == track)
                {
                    source = object["id"];
                    updateName();
                }
            refreshStatus();
            status.setText(text("已提交 · 可 Undo / Redo。点列表名称选择，拖选波形后提升到 TARGET。"),
                           juce::dontSendNotification);
            if (id == 527)
            {
                draftNotice.clear();
                status.setText(text("候选已创建 · 原播放和目标保留 · 整份候选一次 Undo。"), juce::dontSendNotification);
            }
            if (id == 529 || id == 530)
                status.setText(text(id == 529 ? "整组已恢复原播放 · 可一次撤销" : "整组关联已解除 · 音频不变 · 可撤销"),
                               juce::dontSendNotification);
            canvas.repaint();
        }
        catch (const std::exception& e)
        {
            status.setText(text(e.what()), juce::dontSendNotification);
        }
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced(14);
        auto header = r.removeFromTop(32);
        buttons.at(531)->setBounds(header.removeFromRight(104).reduced(2));
        trackChoice.setBounds(header.removeFromRight(220).reduced(2));
        title.setBounds(header);
        auto tools = r.removeFromTop(34);
        name.setBounds(tools.removeFromLeft(170).reduced(2));
        for (int id : {515, 516, 517, 520, 521, 524})
            buttons.at(id)->setBounds(tools.removeFromLeft(94).reduced(2));
        auto actions = r.removeFromTop(34);
        for (int id : {523, 518, 519, 522})
            buttons.at(id)->setBounds(actions.removeFromLeft(118).reduced(2));
        for (int id : {529, 530})
            buttons.at(id)->setBounds(actions.removeFromLeft(128).reduced(2));
        zoom.setBounds(actions.removeFromLeft(160).reduced(3));
        auto listen = r.removeFromTop(34);
        for (int id : {532, 533, 534, 535, 536, 537})
            buttons.at(id)->setBounds(listen.removeFromLeft(id == 534 || id == 535 ? 90 : 148).reduced(2));
        status.setBounds(r.removeFromBottom(34));
        auto candidate = r.removeFromBottom(112);
        auto candidateTools = candidate.removeFromTop(32);
        candidateName.setBounds(candidateTools.removeFromLeft(200).reduced(2));
        for (int id : {525, 526, 527, 528})
            buttons.at(id)->setBounds(candidateTools.removeFromLeft(112).reduced(2));
        draftList.setBounds(candidate.reduced(0, 2));
        scroll.setBounds(r.removeFromBottom(14));
        view.setBounds(r);
        canvas.setSize(std::max(300, view.getMaximumVisibleWidth()),
                       data.is_null() ? 100 : std::max(r.getHeight(), int(data["items"].size()) * 106 + 26));
        layoutTime();
    }

private:
    struct DraftModel final : juce::ListBoxModel
    {
        PlaylistPanel& owner;
        explicit DraftModel(PlaylistPanel& p) : owner(p) {}
        int getNumRows() override
        {
            return int(owner.draft.size());
        }
        juce::String getNameForRow(int row) override
        {
            if (row < 0 || row >= int(owner.draft.size()))
                return {};
            const auto& s = owner.draft[row];
            const auto key = s["track"].get<std::string>() + ":" + s["source"].get<std::string>();
            const auto found = owner.draftLabels.find(key);
            const std::string label = found == owner.draftLabels.end() ? key : found->second;
            return juce::String(row + 1) + "  " + text(label) + "    " +
                   juce::String(s["start_samples"].get<int64_t>() / 48000., 6) + " – " +
                   juce::String(s["end_samples"].get<int64_t>() / 48000., 6) + text(" 秒 · 原工程位置");
        }
        void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
        {
            g.fillAll(selected ? juce::Colour(0xff344252) : juce::Colour(0xff191e25));
            g.setColour(juce::Colours::white);
            g.setFont(12.f);
            g.drawText(getNameForRow(row), 8, 0, width - 16, height, juce::Justification::centredLeft);
        }
        void selectedRowsChanged(int) override
        {
            owner.refreshStatus();
        }
    };
    struct Canvas : juce::Component, juce::ScrollBar::Listener
    {
        PlaylistPanel& owner;
        Waveforms waves;
        bool dragging = false;
        int64_t anchor = 0, first = 0, last = 0;
        explicit Canvas(PlaylistPanel& p) : owner(p), waves([this] { repaint(); })
        {
            setWantsKeyboardFocus(true);
        }
        double x(int64_t sample) const
        {
            return 200. + (sample - owner.offset) / owner.samplesPerPixel;
        }
        int64_t at(float pixel) const
        {
            return std::clamp(int64_t(std::llround(owner.offset + (pixel - 200.) * owner.samplesPerPixel)), first,
                              last);
        }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff191e25));
            if (owner.data.is_null())
                return;
            auto clipBox = [&](const Json& c, int row)
            {
                return juce::Rectangle<int>(
                    int(x(c["start_samples"])), 26 + row * 106 + 22,
                    std::max(1, int(c["length_samples"].get<int64_t>() / owner.samplesPerPixel)), 78);
            };
            g.setColour(juce::Colours::lightgrey);
            g.setFont(11.f);
            for (int px = 200; px < getWidth(); px += 140)
                g.drawText(juce::String((owner.offset + (px - 200) * owner.samplesPerPixel) / 48000., 2) + " s", px, 0,
                           136, 22, juce::Justification::centredLeft);
            int row = 0;
            for (const auto& list : owner.data["items"])
            {
                auto lane = juce::Rectangle<int>(0, 26 + row * 106, getWidth(), 106);
                g.setColour(list["id"] == owner.source ? juce::Colour(0xff344252) : juce::Colour(0xff262d37));
                g.fillRect(lane.removeFromLeft(198));
                g.setColour(juce::Colours::white);
                g.setFont(14.f);
                g.drawText(text(list["name"].get<std::string>()), 8, 32 + row * 106, 185, 24,
                           juce::Justification::centredLeft);
                std::string flags = list["active"].get<bool>() ? "PLAY  " : "";
                if (list["id"] == owner.data["target"])
                    flags += "TARGET  ";
                if (!list.value("linked_set", std::string{}).empty())
                    flags += "LINK  ";
                if (list["locked"].get<bool>())
                    flags += "LOCK";
                g.setColour(accent());
                g.setFont(11.f);
                g.drawText(text(flags), 8, 62 + row * 106, 185, 24, juce::Justification::centredLeft);
                juce::Graphics::ScopedSaveState scope(g);
                g.reduceClipRegion(lane);
                for (const auto& c : list["clips"])
                {
                    const auto box = clipBox(c, row);
                    if (!box.intersects(g.getClipBounds()))
                        continue;
                    g.setColour(c["media_available"].get<bool>() ? juce::Colour(0xff3b666a) : juce::Colour(0xff704044));
                    g.fillRect(box);
                    g.setColour(juce::Colour(0xffa4d5cc));
                    const auto visible = box.getIntersection(lane);
                    const double elapsed = (visible.getX() - box.getX()) * owner.samplesPerPixel / 48000.;
                    waves.draw(g, c, visible.reduced(0, 12), visible.getWidth() * owner.samplesPerPixel / 48000.,
                               elapsed);
                    g.setColour(juce::Colours::white);
                    g.drawText(text(c["name"].get<std::string>()), box.withHeight(18).reduced(3, 0),
                               juce::Justification::centredLeft);
                    if (!c["media_available"].get<bool>())
                        g.drawText(text("媒体缺失"), box, juce::Justification::centred);
                }
                if (list["id"] == owner.source && owner.end > owner.begin)
                {
                    g.setColour(accent().withAlpha(.3f));
                    g.fillRect(juce::Rectangle<float>(float(x(owner.begin)), float(26 + row * 106),
                                                      float((owner.end - owner.begin) / owner.samplesPerPixel), 106.f));
                }
                ++row;
            }
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (owner.data.is_null())
                return;
            if (owner.auditionStatus.value("active", false))
                owner.updateAudition(owner.audition("stop", Json::object()));
            if (isShowing())
                grabKeyboardFocus();
            const int row = (e.y - 26) / 106;
            if (e.y < 26 || row < 0 || row >= int(owner.data["items"].size()))
                return;
            const auto& list = owner.data["items"][row];
            owner.source = list["id"];
            owner.sourceClip.clear();
            const auto retainedBegin = owner.begin, retainedEnd = owner.end;
            owner.begin = owner.end = 0;
            dragging = false;
            owner.updateName();
            if (e.x >= 200)
                for (const auto& c : list["clips"])
                {
                    first = c["start_samples"];
                    last = first + c["length_samples"].get<int64_t>();
                    if (e.x >= x(first) && e.x < x(last))
                    {
                        owner.sourceClip = c["id"];
                        anchor = at(e.position.x);
                        owner.begin = first;
                        owner.end = last;
                        if (e.mods.isShiftDown() && retainedEnd > retainedBegin && retainedBegin >= first &&
                            retainedEnd <= last)
                        {
                            owner.begin = retainedBegin;
                            owner.end = retainedEnd;
                            break;
                        }
                        dragging = true;
                        break;
                    }
                }
            owner.refreshStatus();
            repaint();
        }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (!dragging)
                return;
            const auto point = at(e.position.x);
            owner.begin = std::min(anchor, point);
            owner.end = std::max(anchor, point);
            owner.refreshStatus();
            repaint();
        }
        void mouseUp(const juce::MouseEvent&) override
        {
            dragging = false;
        }
        void scrollBarMoved(juce::ScrollBar*, double start) override
        {
            owner.offset = start;
            repaint();
        }
    };
    const Json& selectedList() const
    {
        for (const auto& list : data["items"])
            if (list["id"] == source)
                return list;
        throw std::runtime_error("所选 Playlist 已不存在");
    }
    void updateName()
    {
        for (const auto& list : data["items"])
            if (list["id"] == source)
                name.setText(text(list["name"].get<std::string>()), false);
    }
    void refreshStatus()
    {
        status.setText(!draftNotice.isEmpty() ? draftNotice
                       : end > begin
                           ? text("选区 ") + juce::String(begin / 48000., 6) + " – " + juce::String(end / 48000., 6) +
                                 text(" 秒 · 提升仅替换 TARGET 的这一区间，源 Take 保留")
                           : text("点选列表；用于播放切换当前 Take，空格试听。拖选波形 → 提升选区；新建 Comp "
                                  "复制当前列表并保留原播放。") +
                                 (draft.empty()
                                      ? juce::String{}
                                      : text(" · 未提交候选 ") + juce::String(int(draft.size())) + text(" 段")),
                       juce::dontSendNotification);
        if (auditionStatus.value("active", false) && auditionStatus.value("state", std::string{}) == "preparing")
        {
            const auto p = auditionStatus.value("preparation", Json::object());
            status.setText(text("正在准备试听来源 · ") + juce::String(p.value("ready_sources", 0)) + "/" +
                               juce::String(p.value("total_sources", 0)) + text(" 源 · ") +
                               (p.value("progress_available", false)
                                    ? juce::String(p.value("progress", 0.) * 100., 0) + "%"
                                    : text("等待作业")) +
                               text(" · 完成后开始所选范围 · 停止可取消"),
                           juce::dontSendNotification);
        }
        else if (auditionStatus.value("active", false))
            status.setText(
                text(auditionStatus.value("state", std::string{}) == "requested" ? "等待实际音频回调 · "
                                                                                 : "正在试听 · ") +
                    text(auditionStatus.value("mode", std::string{}) == "draft" ? "候选草稿（未创建）" : "来源列表") +
                    text(" · 仅所选来源及其路由/处理 · 输入/节拍器关闭 · 停止后返回原光标"),
                juce::dontSendNotification);
        else if (auditionStatus.value("state", std::string{}) == "failed")
        {
            const auto reason = auditionStatus.value("reason", std::string{});
            const auto explanation =
                reason == "source_preparation_failed"    ? "来源音频准备失败，未开始试听"
                : reason == "source_preparation_timeout" ? "来源音频准备超时，可检查磁盘后重试"
                : reason == "project_changed_during_preparation" || reason == "project_or_device_changed"
                    ? "工程或设备已变化，已取消旧试听"
                    : reason;
            status.setText(text("试听失败：") + text(explanation), juce::dontSendNotification);
        }
        std::set<std::string> members;
        for (const auto& s : draft)
            members.insert(s["track"].get<std::string>());
        buttons.at(527)->setButtonText(text(members.size() > 1 ? "创建关联 Comp" : "创建候选"));
        for (auto& [id, b] : buttons)
            b->setEnabled(canExecute(id));
    }
    void layoutTime()
    {
        const auto visible = duration / zoom.getValue();
        samplesPerPixel = visible / std::max(1, canvas.getWidth() - 200);
        offset = std::clamp(offset, 0., std::max(0., duration - visible));
        scroll.setRangeLimits(0., double(duration));
        scroll.setCurrentRange(offset, visible, juce::dontSendNotification);
        canvas.repaint();
    }
    class NameEditor : public juce::TextEditor
    {
    public:
        void connect(juce::ApplicationCommandManager& m, int enter = 517)
        {
            manager = &m;
            enterCommand = enter;
        }
        bool keyPressed(const juce::KeyPress& key) override
        {
            if (manager)
            {
                const int command = key == juce::KeyPress::escapeKey ? 524
                                    : key == juce::KeyPress::returnKey
                                        ? enterCommand
                                        : manager->getKeyMappings()->findCommandForKeyPress(key);
                if (command >= 515 && command <= 537)
                {
                    manager->invokeDirectly(command, false);
                    return true;
                }
            }
            return juce::TextEditor::keyPressed(key);
        }

    private:
        juce::ApplicationCommandManager* manager = nullptr;
        int enterCommand = 517;
    };
    Submit submit;
    Audition audition;
    Json auditionStatus = {{"active", false}};
    std::function<void()> close;
    std::function<Json(const std::string&)> query;
    juce::ComboBox trackChoice;
    std::map<std::string, std::string> draftLabels;
    std::string track, source, sourceClip;
    Json data = nullptr;
    int64_t begin = 0, end = 0, duration = 480000;
    double offset = 0, samplesPerPixel = 480.;
    juce::Label title, status;
    NameEditor name;
    std::map<int, std::unique_ptr<juce::TextButton>> buttons;
    Canvas canvas;
    Json draft = Json::array();
    juce::String draftNotice;
    DraftModel draftModel;
    juce::ListBox draftList;
    NameEditor candidateName;
    juce::Viewport view;
    juce::ScrollBar scroll{false};
    juce::Slider zoom;
};
} // namespace ndaw::desktop
