#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2::curve_edit
{
void attachCurve(te::AutomationCurve&, juce::UndoManager*);
constexpr size_t maximumPoints = 65536, maximumDerived = 8192;
constexpr double relativeError = 1e-7;
struct Point
{
    double time;
    float value, curve;
    std::string id;
};
std::vector<Point> collapse(const std::vector<Point>&, double first, double last, double tolerance);
std::vector<Point> clearRange(const std::vector<Point>&, double first, double last, double tolerance, bool cut);
Json serialise(const std::vector<Point>&);
std::vector<Point> fragment(const Point&, const Point&, double low, double high, double tolerance);
std::vector<Point> read(const juce::ValueTree&);
std::vector<Point> slice(const std::vector<Point>&, double low, double high, double tolerance);
float nativeValue(const std::vector<Point>&, double at);
double adjacentTimelineSample(double at, bool before);
std::vector<Point> startSlice(const std::vector<Point>&, double low, double high, double tolerance,
                              bool destinationZero);
std::vector<Point> musicalSuffix(const std::vector<Point>&, const tracktion::tempo::Sequence&, double sourceStart,
                                 double destinationStart, double tolerance);
std::vector<Point> musicalCollapse(const std::vector<Point>&, const tracktion::tempo::Sequence&, double first,
                                   double last, double tolerance);
std::vector<Point> musicalSlice(const std::vector<Point>&, const tracktion::tempo::Sequence& source,
                                const tracktion::tempo::Sequence& destination, double originBeat, double start,
                                double high, double tolerance);
} // namespace ndaw::v2::curve_edit
