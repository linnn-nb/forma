#pragma once
#include "Theme.h"
#include "TrackHeader.h"
#include "Waveforms.h"
namespace ndaw::desktop
{
class EditWindow final : public juce::Component
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
    }
    void update(const Json& value, const std::string& selection, const Json& grid,
                const std::string& clipSelection = {})
    {
        facts = value;
        facts["tracks"] = Json::array();
        for (const auto& t : value["tracks"])
            if (!t.value("edit_hidden", false))
                facts["tracks"].push_back(t);
        selected = selection;
        selectedClip = clipSelection;
        this->grid = grid;
        std::vector<std::string> ids;
        for (const auto& t : facts["tracks"])
            ids.push_back(t["id"]);
        if (ids != trackIDs)
        {
            trackIDs = ids;
            controls.clear();
            for (auto& id : ids)
            {
                auto c = std::make_unique<TrackHeader>(id, false, write, select);
                addAndMakeVisible(*c);
                controls.push_back(std::move(c));
            }
        }
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto item = facts["tracks"][i];
            item["playing"] = facts["playing"];
            item["automation_writing"] = !facts["automation_capture"].is_null();
            item["recording"] = !facts["recording_capture"].is_null();
            controls[i]->update(item, trackIDs[i] == selected);
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
                    b->onClick = [this, id, owner = t["id"].get<std::string>()]
                    {
                        select(owner);
                        if (selectClip)
                            selectClip(id);
                    };
                    b->setTooltip(text("选择片段 · 波形中拖动移动，左右边缘拖动修剪"));
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
        repaint();
    }
    int visibleRows() const
    {
        return int(trackIDs.size());
    }
    double duration() const
    {
        return std::max(10., facts.value("length_samples", int64_t(0)) / 48000. * 1.05);
    }
    juce::Rectangle<int> clipRect(const Json& c, int row) const
    {
        double d = duration(), w = getWidth() - 262;
        return {250 + int(c["start_samples"].get<int64_t>() / 48000. / d * w), 44 + row * 144,
                std::max(3, int(c["length_samples"].get<int64_t>() / 48000. / d * w)), 118};
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
        g.setColour(juce::Colour(0xff29333f));
        g.fillRect(0, 0, getWidth(), 32);
        g.setFont(juce::FontOptions(11));
        g.setColour(juce::Colour(0xffb9c8d8));
        g.drawText(text("TRACKS / 轨道"), 12, 0, 210, 32, juce::Justification::left);
        for (size_t i = 0; i < facts.value("tracks", Json::array()).size(); ++i)
        {
            g.setColour(juce::Colour(facts["tracks"][i]["id"] == selected ? 0xff21313d : 0xff1c2530));
            g.fillRect(250, 32 + int(i) * 144, getWidth() - 250, 143);
        }
        for (const auto& line : grid)
        {
            int x = 250 + int(line["samples"].get<int64_t>() / 48000. / duration() * (getWidth() - 262));
            bool bar = line["bar_line"];
            g.setColour(juce::Colour(bar ? 0xff536575 : 0xff303b49));
            g.drawVerticalLine(x, 32, float(getHeight()));
            if (bar)
            {
                g.setColour(juce::Colour(0xffacbbcc));
                g.drawText(juce::String(line["bar"].get<int>()) + " |", x + 4, 0, 50, 30, juce::Justification::left);
            }
        }
        if (auto range = facts.value("time_selection", Json(nullptr)); !range.is_null())
        {
            const auto left =
                           250 + int(range["start_samples"].get<int64_t>() / 48000. / duration() * (getWidth() - 262)),
                       right =
                           250 + int(range["end_samples"].get<int64_t>() / 48000. / duration() * (getWidth() - 262));
            g.setColour(accent().withAlpha(.14f));
            g.fillRect(left, 0, std::max(1, right - left), getHeight());
            g.setColour(accent());
            g.drawVerticalLine(left, 0, float(getHeight()));
            g.drawVerticalLine(right, 0, float(getHeight()));
        }
        for (size_t i = 0; i < facts.value("tracks", Json::array()).size(); ++i)
        {
            int y = 32 + int(i) * 144;
            const auto& t = facts["tracks"][i];
            g.setColour(juce::Colour(0xff33404e));
            g.drawHorizontalLine(y + 143, 0, float(getWidth()));
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
                    waves.draw(g, c, rect.reduced(0, 25), length);
                    g.setColour(juce::Colour(0xffe6e1b2));
                    double in = c.value("fade_in_samples", int64_t(0)) / 48000. / length,
                           out = c.value("fade_out_samples", int64_t(0)) / 48000. / length;
                    drawFade(g, rect, in, c.value("fade_in_curve", std::string("linear")), true);
                    drawFade(g, rect, out, c.value("fade_out_curve", std::string("linear")), false);
                }
                if (c["id"] == selectedClip)
                {
                    g.setColour(accent());
                    g.drawRoundedRectangle(rect.toFloat(), 3, 2);
                    g.fillRect(rect.getX(), rect.getY() + 25, 3, rect.getHeight() - 25);
                    g.fillRect(rect.getRight() - 3, rect.getY() + 25, 3, rect.getHeight() - 25);
                }
            }
        }
        if (!drag.is_null() && dragged)
        {
            auto preview = drag["clip"];
            preview["start_samples"] = dragStart;
            preview["length_samples"] = dragEnd - dragStart;
            g.setColour(accent().withAlpha(.25f));
            g.fillRect(clipRect(preview, drag["row"]));
        }
        int x = 250 + int(facts.value("position_samples", int64_t(0)) / 48000. / duration() * (getWidth() - 262));
        g.setColour(juce::Colour(0xffedca72));
        g.drawVerticalLine(x, 0, float(getHeight()));
        if (facts.value("tracks", Json::array()).empty())
        {
            g.setColour(juce::Colour(0xffb2c3d4));
            g.setFont(juce::FontOptions(18));
            g.drawText(text("导入音频，开始制作"), 270, 90, getWidth() - 290, 32, juce::Justification::centred);
            g.setFont(juce::FontOptions(13));
            g.drawText(text("⌘I 导入 · 空格播放 / 停止 · ⌘Z 撤销"), 270, 130, getWidth() - 290, 26,
                       juce::Justification::centred);
        }
    }
    void resized() override
    {
        for (size_t i = 0; i < controls.size(); ++i)
            controls[i]->setBounds(0, 32 + int(i) * 144, 242, 143);
        for (size_t i = 0; i < facts.value("tracks", Json::array()).size(); ++i)
            for (const auto& c : facts["tracks"][i]["clips"])
                if (headers.contains(c["id"]))
                    headers.at(c["id"])->setBounds(clipRect(c, int(i)).reduced(3).removeFromTop(20));
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        drag = nullptr;
        dragged = false;
        if (e.x < 250)
            return;
        int row = (e.y - 32) / 144;
        auto sample = sampleAt(e.x);
        if (e.y >= 32 && row >= 0 && row < int(trackIDs.size()))
        {
            select(trackIDs[row]);
            for (const auto c : facts["tracks"][row]["clips"])
                if (clipRect(c, row).contains(e.getPosition()))
                {
                    if (selectClip)
                        selectClip(c["id"]);
                    if (c["kind"] == "audio" && c.value("editable_audio", false) && !c.value("locked", false) &&
                        !facts.value("playing", false))
                    {
                        auto r = clipRect(c, row);
                        drag = {{"clip", c},
                                {"revision", facts["revision"]},
                                {"row", row},
                                {"mode", e.x < r.getX() + 8        ? "left"
                                         : e.x >= r.getRight() - 8 ? "right"
                                                                   : "move"}};
                        dragX = e.x;
                        dragStart = c["start_samples"];
                        dragEnd = dragStart + c["length_samples"].get<int64_t>();
                        dragScale = duration() * 48000 / (getWidth() - 262);
                    }
                    break;
                }
        }
        seek(sample);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (drag.is_null())
            return;
        int64_t delta = std::llround((e.x - dragX) * dragScale);
        const auto& c = drag["clip"];
        int64_t start = c["start_samples"], length = c["length_samples"], offset = c["source_offset_samples"],
                source =
                    std::llround(c["source_frames"].get<int64_t>() / c["source_sample_rate"].get<double>() * 48000);
        std::string mode = drag["mode"];
        if (mode == "move")
        {
            dragStart = std::max(int64_t(0), start + delta);
            dragEnd = dragStart + length;
        }
        else if (mode == "left")
        {
            dragStart = std::clamp(start + delta, std::max(int64_t(0), start - offset), start + length - 1);
            dragEnd = start + length;
        }
        else
        {
            dragStart = start;
            dragEnd = std::clamp(start + length + delta, start + 1, start + source - offset);
        }
        dragged = std::abs(e.x - dragX) >= 3;
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (drag.is_null())
            return;
        auto captured = drag;
        drag = nullptr;
        repaint();
        if (!dragged || !clipWrite)
            return;
        std::string mode = captured["mode"];
        if (mode == "move")
            clipWrite("clip.move", {{"clip", captured["clip"]["id"]}, {"position_samples", dragStart}},
                      captured["revision"]);
        else
            clipWrite("clip.trim",
                      {{"clip", captured["clip"]["id"]}, {"start_samples", dragStart}, {"end_samples", dragEnd}},
                      captured["revision"]);
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.x < 250 || e.y < 32)
            return;
        int row = (e.y - 32) / 144;
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
        return std::llround(std::max(0., (x - 250) / double(getWidth() - 262) * duration() * 48000));
    }
    Writer write;
    std::function<void(std::string)> select;
    std::function<void(int64_t)> seek;
    Waveforms& waves;
    std::function<void(std::string)> openMidi, selectClip;
    std::function<void(const std::string&, Json, uint64_t)> clipWrite;
    Json facts = Json::object(), grid = Json::array(), drag = nullptr;
    std::string selected, selectedClip;
    int dragX = 0;
    int64_t dragStart = 0, dragEnd = 0;
    double dragScale = 0;
    bool dragged = false;
    std::vector<std::string> trackIDs;
    std::vector<std::unique_ptr<TrackHeader>> controls;
    std::map<std::string, std::unique_ptr<juce::TextButton>> headers;
};

} // namespace ndaw::desktop
