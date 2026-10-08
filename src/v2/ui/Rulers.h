#pragma once
#include "TimelineCoordinates.h"
namespace ndaw::desktop
{
// Display-only scales consume L1 TempoSequence facts and the same session-sample axis as clips.
class Rulers
{
public:
    struct Entry
    {
        const char* key;
        const char* title;
        int command, mainCommand, height;
    };
    static const std::vector<Entry>& entries()
    {
        static const std::vector<Entry> list{{"bars_beats", "BARS | BEATS", 154, 166, 29},
                                             {"min_sec", "MIN : SEC", 155, 167, 29},
                                             {"timecode", "TIMECODE", 156, 168, 29},
                                             {"samples", "SAMPLES · 48k", 157, 169, 29},
                                             {"markers", "MARKERS", 158, 0, 20},
                                             {"tempo", "TEMPO", 159, 0, 20},
                                             {"meter", "METER", 160, 0, 20}};
        return list;
    }
    static Json defaults()
    {
        return {{"bars_beats", true}, {"min_sec", true}, {"timecode", false}, {"samples", false},
                {"markers", true},    {"tempo", false},  {"meter", false}};
    }
    static bool shown(const Json& view, const std::string& key)
    {
        return view.value("rulers", defaults()).value(key, false);
    }
    static int top(const Json& view, const std::string& key)
    {
        int y = 0;
        for (const auto& entry : entries())
        {
            if (entry.key == key)
                return shown(view, key) ? y : -1;
            if (shown(view, entry.key))
                y += entry.height;
        }
        return -1;
    }
    static int height(const Json& view)
    {
        int h = 0;
        for (const auto& entry : entries())
            if (shown(view, entry.key))
                h += entry.height;
        return h;
    }
    static const Entry* at(const Json& view, int y)
    {
        for (const auto& e : entries())
        {
            const int first = top(view, e.key);
            if (first >= 0 && y >= first && y < first + e.height)
                return &e;
        }
        return nullptr;
    }
    static int64_t niceInteger(double raw)
    {
        const double magnitude = std::pow(10., std::floor(std::log10(std::max(1., raw))));
        return std::max(int64_t(1), std::llround((raw / magnitude <= 1   ? 1
                                                  : raw / magnitude <= 2 ? 2
                                                  : raw / magnitude <= 5 ? 5
                                                                         : 10) *
                                                 magnitude));
    }
    static Json ticks(const TimelineCoordinates& axis, const std::string& type, int fps)
    {
        if (fps != 24 && fps != 25 && fps != 30)
            throw std::runtime_error("unsupported ruler display frame rate");
        const int labels = std::max(1, int(axis.width) / (type == "timecode" ? 110 : 88));
        int64_t step = 1;
        if (type == "samples")
            step = niceInteger(double(axis.span) / labels);
        else if (type == "timecode")
            step = niceInteger(double(axis.span) / (48000 / fps) / labels) * (48000 / fps);
        else
        {
            const double raw = double(axis.span) / 48000 / labels;
            const double magnitude = std::pow(10., std::floor(std::log10(std::max(1. / 48000, raw))));
            step = std::max(int64_t(1), std::llround((raw / magnitude <= 1   ? 1
                                                      : raw / magnitude <= 2 ? 2
                                                      : raw / magnitude <= 5 ? 5
                                                                             : 10) *
                                                     magnitude * 48000));
        }
        Json result = Json::array();
        for (int64_t sample = (axis.start / step) * step; sample <= axis.start + axis.span && result.size() < 4096;
             sample += step)
        {
            const auto label = type == "samples"    ? juce::String(sample)
                               : type == "timecode" ? TimelineCoordinates::frames(sample, fps)
                                                    : TimelineCoordinates::minutesSeconds(sample);
            result.push_back({{"samples", sample}, {"label", label.toStdString()}});
        }
        return result;
    }
    static void draw(juce::Graphics& g, const TimelineCoordinates& axis, const Json& grid, int width, const Json& view,
                     const Json& facts, const Json& context)
    {
        g.setColour(juce::Colour(0xff30343b));
        g.fillRect(0, 0, width, height(view));
        const int fps = view.value("timecode_fps", 24);
        for (const auto& e : entries())
        {
            const int y = top(view, e.key);
            if (y < 0)
                continue;
            const std::string type = e.key;
            const bool primary = view.value("main_time_scale", std::string("min_sec")) == type;
            g.setColour(primary ? accent() : juce::Colour(0xffadb5c1));
            g.setFont(juce::FontOptions(10));
            auto title = text(e.title);
            if (type == "timecode")
                title += " " + juce::String(fps) + " NDF";
            g.drawText(title, 46, y + 1, std::max(1, int(axis.left) - 58), e.height - 2, juce::Justification::right);
            juce::Graphics::ScopedSaveState content(g);
            g.reduceClipRegion(int(axis.left), y, std::max(1, width - int(axis.left)), e.height);
            if (type == "bars_beats")
            {
                int last = -10000;
                for (const auto& line : grid)
                {
                    const int x = int(std::round(axis.pixelAt(line["samples"])));
                    if (x < axis.left || x - last < 48)
                        continue;
                    const auto beat = line["beat_in_bar"].get<double>();
                    g.setColour(line["bar_line"].get<bool>() ? juce::Colour(0xffd4dbe2) : juce::Colour(0xff93a1ae));
                    g.drawText(juce::String(line["bar"].get<int>()) + " | " +
                                   juce::String(beat, std::abs(beat - std::round(beat)) < 1e-6 ? 0 : 2),
                               x + 4, y + 1, 76, 20, juce::Justification::left);
                    g.drawVerticalLine(x, float(y + 21), float(y + 28));
                    last = x;
                }
            }
            else if (e.mainCommand != 0)
            {
                for (const auto& tick : ticks(axis, type, fps))
                {
                    const int x = int(std::round(axis.pixelAt(tick["samples"])));
                    if (x < axis.left)
                        continue;
                    g.setColour(juce::Colour(0xffb0bdc9));
                    g.drawText(text(tick["label"].get<std::string>()), x + 4, y + 1, type == "timecode" ? 108 : 86, 20,
                               juce::Justification::left);
                    g.drawVerticalLine(x, float(y + 21), float(y + 28));
                }
            }
            else if (type == "tempo" || type == "meter")
            {
                const auto values =
                    facts.value("music", Json::object()).value(type == "tempo" ? "tempos" : "meters", Json::array());
                int last = -10000;
                auto label = [&](const Json& value)
                {
                    return type == "tempo" ? juce::String(value.at("bpm").get<double>(), 2) + " BPM"
                                           : juce::String(value.at("numerator").get<int>()) + "/" +
                                                 juce::String(value.at("denominator").get<int>());
                };
                if (axis.start > 0 && context.contains("bpm"))
                {
                    g.setColour(juce::Colour(0xffb9d2df));
                    g.drawText(label(context), int(axis.left) + 4, y, 100, e.height, juce::Justification::left);
                    last = int(axis.left);
                }
                for (const auto& value : values)
                {
                    const int x = int(std::round(axis.pixelAt(value.at("position_samples"))));
                    if (x < axis.left || x > width)
                        continue;
                    g.setColour(accent());
                    g.drawVerticalLine(x, float(y + 2), float(y + e.height - 2));
                    if (x - last >= 86)
                    {
                        g.drawText(label(value), x + 4, y, 96, e.height, juce::Justification::left);
                        last = x;
                    }
                }
            }
            g.setColour(juce::Colour(0xff454b56));
            g.drawHorizontalLine(y + e.height - 1, 0, float(width));
        }
    }
};
} // namespace ndaw::desktop
