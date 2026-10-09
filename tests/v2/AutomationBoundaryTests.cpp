#include "AutomationCurveEdit.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static te::Edit& edit(Commands& c)
    {
        return *c.edit;
    }
};
} // namespace ndaw::v2
namespace
{
class Storage final : public te::PropertyStorage
{
public:
    Storage()
        : PropertyStorage("Forma curve boundary qualification"),
          folder(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(juce::Uuid().toString()))
    {
        folder.createDirectory();
    }
    ~Storage() override
    {
        folder.deleteRecursively();
    }
    juce::File getAppPrefsFolder() override
    {
        return folder;
    }

private:
    juce::File folder;
};
juce::ValueTree state(const std::vector<curve_edit::Point>& points)
{
    juce::ValueTree result(te::IDs::AUTOMATIONCURVE);
    for (const auto& p : points)
    {
        juce::ValueTree point(te::IDs::POINT);
        point.setProperty(te::IDs::t, p.time, nullptr);
        point.setProperty(te::IDs::v, p.value, nullptr);
        point.setProperty(te::IDs::c, p.curve, nullptr);
        result.addChild(point, -1, nullptr);
    }
    return result;
}
auto time(double at)
{
    return tracktion::TimePosition::fromSeconds(at);
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        Commands commands(false, std::make_unique<Storage>());
        auto& edit = AudioDeviceTestAccess::edit(commands);
        size_t probes = 0, cases = 0;
        double maximumError = 0;
        edit.tempoSequence.toBeats(time(0.));
        const auto& tempo = edit.tempoSequence.getInternalSequence();
        for (bool musical : {false, true})
            for (float shape : {.75f, -.75f, 0.f, .25f, -.25f, .5f, -.5f, 1.f, -1.f})
                for (double phase : {0., .37 / 48000., 1e-12, -1e-12})
                    for (const auto bounds :
                         {std::pair{.5, 3.}, std::pair{0., 1.5}, std::pair{1., 2.}, std::pair{2., 3.}})
                        for (double destination : {0., 8.})
                        {
                            const std::vector<curve_edit::Point> source{
                                {0, .2f, shape, "first"}, {2. + phase, .8f, -shape, "middle"}, {4., .35f, 0, "last"}};
                            const auto [low, high] = bounds;
                            const double offset = destination - low;
                            auto projected =
                                musical ? curve_edit::musicalSlice(source, tempo, tempo,
                                                                   tempo.toBeats(time(low)).inBeats(), destination,
                                                                   destination + high - low, curve_edit::relativeError)
                                        : curve_edit::startSlice(source, low, high, curve_edit::relativeError,
                                                                 destination == 0);
                            if (!musical)
                                for (auto& p : projected)
                                    p.time += offset;
                            if (destination > 0)
                                projected.insert(projected.begin(), {destination - 1 / 48000., .2f, 0, {}});
                            // Owned isolated native curves; production Edit is never mutated by the test.
                            juce::ValueTree parent("QUALIFICATION"), a = state(source), b = state(projected);
                            parent.addChild(a, -1, nullptr);
                            parent.addChild(b, -1, nullptr);
                            te::AutomationCurve original(edit, te::AutomationCurve::TimeBase::time, parent, a),
                                moved(edit, te::AutomationCurve::TimeBase::time, parent, b);
                            te::AutomationIterator expected(edit, original), actual(edit, moved);
                            auto probe = [&](double sourceTime, double targetTime)
                            {
                                if (targetTime < destination || targetTime > destination + high - low)
                                    return;
                                expected.setPosition(time(sourceTime));
                                actual.setPosition(time(targetTime));
                                const float wanted = expected.getCurrentValue(), got = actual.getCurrentValue();
                                const double ulp = std::max(std::abs(double(std::nextafter(wanted, INFINITY)) - wanted),
                                                            std::abs(double(std::nextafter(got, INFINITY)) - got));
                                const double error = std::abs(double(wanted) - got);
                                maximumError = std::max(maximumError, error);
                                if (error > curve_edit::relativeError + 2 * ulp)
                                    throw std::runtime_error("native slice mismatch " +
                                                             Json{{"musical", musical},
                                                                  {"shape", shape},
                                                                  {"phase", phase},
                                                                  {"source", sourceTime},
                                                                  {"destination", targetTime},
                                                                  {"expected", wanted},
                                                                  {"actual", got},
                                                                  {"error", error}}
                                                                 .dump());
                                ++probes;
                            };
                            const auto first = int64_t(std::ceil(destination * 48000.)),
                                       last = int64_t(std::floor((destination + high - low) * 48000.));
                            for (int64_t sample = first; sample <= last; sample += 13)
                                probe(sample / 48000. - offset, sample / 48000.);
                            // Probe every immediate canonical sample around each true native jump.
                            for (const auto& point : source)
                            {
                                const double at = point.time + offset;
                                const auto sample = int64_t(std::floor(at * 48000.));
                                for (int n = -2; n <= 2; ++n)
                                    probe((sample + n) / 48000. - offset, (sample + n) / 48000.);
                                if (point.time >= low && point.time <= high)
                                    probe(point.time, at);
                            }
                            ++cases;
                        }
        Json report{
            {"state", "passed"},
            {"checks", cases},
            {"probes", probes},
            {"maximum_native_error", maximumError},
            {"budget", "1e-7 parameter span plus two float ULP; unchanged"},
            {"scope",
             "48 kHz timeline samples, fractionally placed native points, positive/negative strong curves and steps"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
