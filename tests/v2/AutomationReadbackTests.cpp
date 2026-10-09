#include <nativedaw/v2/EngineCommands.h>
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
int checks = 0;
Json operation(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
class Storage final : public te::PropertyStorage
{
public:
    Storage()
        : PropertyStorage("Forma readback qualification"),
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
juce::ValueTree point(double t, float v, float c)
{
    juce::ValueTree p(te::IDs::POINT);
    p.setProperty(te::IDs::t, t, nullptr);
    p.setProperty(te::IDs::v, v, nullptr);
    p.setProperty(te::IDs::c, c, nullptr);
    return p;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        Commands c(false, std::make_unique<Storage>());
        c.commit(c.makePlan("human",
                            Json::array({operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}}),
                                         operation("tempo.event.create", {{"beat_position", 16.}, {"bpm", 180.}}),
                                         operation("meter.event.create",
                                                   {{"beat_position", 16.}, {"numerator", 7}, {"denominator", 8}})})));
        auto& edit = AudioDeviceTestAccess::edit(c);
        Json cases = Json::array();
        size_t samples = 0;
        for (bool musical : {false, true})
            for (bool reverse : {false, true})
                for (float shape : {.25f, 0.f, -.25f, .5f, -.5f, .75f, -.75f, 1.f, -1.f})
                {
                    // Isolated native ValueTree, never written into the application's Edit.
                    juce::ValueTree parent("QUALIFICATION"), state(te::IDs::AUTOMATIONCURVE);
                    parent.addChild(state, -1, nullptr);
                    // Beat curves span both Tempo changes and the 7/8 event,
                    // with changes inside curve segments, not just at their ends.
                    const double factor = musical ? 6. : 1.;
                    state.addChild(point(0, reverse ? .8f : .2f, shape), -1, nullptr);
                    state.addChild(point(2 * factor, reverse ? .2f : .8f, -shape), -1, nullptr);
                    state.addChild(point(4 * factor, .4f, 0), -1, nullptr);
                    te::AutomationCurve curve(
                        edit, musical ? te::AutomationCurve::TimeBase::beats : te::AutomationCurve::TimeBase::time,
                        parent, state);
                    const auto original = state.createXml()->toString();
                    const auto control = curve.getBezierPoint(0);
                    const auto expectedControl = tracktion::core::getBezierPoint(0., curve.getPointValue(0), 2 * factor,
                                                                                 curve.getPointValue(1), shape);
                    check(control.time.isBeats() == musical && toUnderlying(control.time) == expectedControl.first &&
                              control.value == float(expectedControl.second),
                          "public Bezier control keeps native coefficient and the curve's own timebase");
                    te::AutomationIterator playback(edit, curve);
                    auto probe = [&](tracktion::TimePosition t)
                    {
                        playback.setPosition(t);
                        const float expected = playback.getCurrentValue();
                        const float seconds = curve.getValueAt(t, .3f),
                                    typedTime = curve.getValueAt(tracktion::EditPosition(t), .3f);
                        const auto beats = edit.tempoSequence.toBeats(t);
                        const float typedBeats = curve.getValueAt(tracktion::EditPosition(beats), .3f);
                        playback.setPosition(beats);
                        const float expectedBeats = playback.getCurrentValue();
                        if (seconds != expected || typedTime != expected || typedBeats != expectedBeats)
                            throw std::runtime_error(
                                "model/playback mismatch: shape=" + std::to_string(shape) +
                                " musical=" + std::to_string(musical) + " at=" + std::to_string(t.inSeconds()) +
                                " expected=" + std::to_string(expected) + " seconds=" + std::to_string(seconds) +
                                " typed_time=" + std::to_string(typedTime) +
                                " typed_beats=" + std::to_string(typedBeats));
                        ++samples;
                    };
                    // Equality required, not a widened numerical tolerance.
                    for (int64_t at = 0; at <= 720000; at += 17)
                        probe(tracktion::TimePosition::fromSeconds(at / 48000.));
                    for (double raw : {0., 2 * factor, 4 * factor})
                    {
                        const auto t = musical ? edit.tempoSequence.toTime(tracktion::BeatPosition::fromBeats(raw))
                                               : tracktion::TimePosition::fromSeconds(raw);
                        for (double offset : {-1 / 48000., 0., 1 / 48000.})
                            probe(t + tracktion::TimeDuration::fromSeconds(offset));
                    }
                    check(state.createXml()->toString() == original,
                          "model readback leaves actual native curve state unchanged");
                    check(true, "seconds, typed-time and typed-beat reads agree with real native playback");
                    cases.push_back({{"musical", musical}, {"reverse", reverse}, {"shape", shape}});
                }
        // Native coincident-point ordering, including the first point.
        for (bool musical : {false, true})
        {
            juce::ValueTree parent("QUALIFICATION"), state(te::IDs::AUTOMATIONCURVE);
            parent.addChild(state, -1, nullptr);
            for (auto p : {point(0, .2f, 0), point(0, .7f, 1), point(2, .8f, -1), point(2, .3f, 0), point(4, .4f, 0)})
                state.addChild(p, -1, nullptr);
            te::AutomationCurve curve(
                edit, musical ? te::AutomationCurve::TimeBase::beats : te::AutomationCurve::TimeBase::time, parent,
                state);
            te::AutomationIterator playback(edit, curve);
            for (double raw : {0., 1., 2., 3., 4.})
            {
                auto t = musical ? edit.tempoSequence.toTime(tracktion::BeatPosition::fromBeats(raw))
                                 : tracktion::TimePosition::fromSeconds(raw);
                playback.setPosition(t);
                check(curve.getValueAt(t, .5f) == playback.getCurrentValue(),
                      "coincident native points retain playback boundary ordering");
            }
        }
        juce::ValueTree parent("QUALIFICATION"), state(te::IDs::AUTOMATIONCURVE);
        parent.addChild(state, -1, nullptr);
        te::AutomationCurve empty(edit, te::AutomationCurve::TimeBase::time, parent, state);
        check(empty.getValueAt(tracktion::TimePosition::fromSeconds(1), .123f) == .123f,
              "empty native curve retains explicit default value");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"probes", samples},
                    {"cases", cases},
                    {"budget", "exact native float equality for every getter/playback comparison"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
            if (!out)
                throw std::runtime_error("readback report write failed");
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
