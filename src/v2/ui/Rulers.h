#pragma once
#include "TimelineCoordinates.h"
namespace ndaw::desktop
{
// The displayed grid is obtained from L1 TempoSequence; no invented beat marks.
class Rulers
{
public:
    static constexpr int height = 58;
    static void draw(juce::Graphics& g, const TimelineCoordinates& axis, const Json& grid, int canvasWidth)
    {
        g.setColour(juce::Colour(0xff30343b));
        g.fillRect(0, 0, canvasWidth, height);
        g.setFont(juce::FontOptions(10));
        g.setColour(juce::Colour(0xffadb5c1));
        g.drawText("BARS | BEATS", 12, 3, int(axis.left) - 18, 23, juce::Justification::right);
        g.drawText("MIN : SEC", 12, 29, int(axis.left) - 18, 23, juce::Justification::right);
        for (const auto& line : grid)
            if (line["bar_line"].get<bool>())
            {
                const int x = int(std::round(axis.pixelAt(line["samples"])));
                if (x < int(axis.left) || x > canvasWidth)
                    continue;
                g.setColour(juce::Colour(0xffcbd1db));
                g.drawText(juce::String(line["bar"].get<int>()), x + 4, 3, 50, 23, juce::Justification::left);
            }
        const double seconds = axis.span / 48000., raw = seconds / 8.;
        const double magnitude = std::pow(10., std::floor(std::log10(std::max(.001, raw))));
        const double step = (raw / magnitude <= 1   ? 1
                             : raw / magnitude <= 2 ? 2
                             : raw / magnitude <= 5 ? 5
                                                    : 10) *
                            magnitude;
        const int64_t tick = std::max(int64_t(1), std::llround(step * 48000));
        const int64_t first = (axis.start / tick) * tick;
        for (auto sample = first; sample <= axis.start + axis.span; sample += tick)
        {
            const int x = int(std::round(axis.pixelAt(sample)));
            if (x < int(axis.left) || x > canvasWidth)
                continue;
            g.setColour(juce::Colour(0xff96a0b0));
            g.drawText(TimelineCoordinates::minutesSeconds(sample).dropLastCharacters(4), x + 4, 29, 62, 22,
                       juce::Justification::left);
            g.drawVerticalLine(x, 46, 57);
        }
        g.setColour(juce::Colour(0xff454b56));
        g.drawHorizontalLine(28, 0, float(canvasWidth));
        g.drawHorizontalLine(height - 1, 0, float(canvasWidth));
    }
};
} // namespace ndaw::desktop
