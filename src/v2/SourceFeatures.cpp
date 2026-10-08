#include <nativedaw/v2/SourceFeatures.h>
#include <juce_cryptography/juce_cryptography.h>
namespace ndaw::v2::analysis
{
namespace
{
void need(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
} // namespace
Json sourceProfileSchema()
{
    const auto number = [](double value, double lo, double hi)
    { return Json{{"type", "number"}, {"default", value}, {"minimum", lo}, {"maximum", hi}}; };
    return {{"type", "object"},
            {"additionalProperties", false},
            {"properties",
             {{"silence_threshold_dbfs", number(-60, -120, -12)},
              {"minimum_silence_ms", number(100, 20, 10000)},
              {"transient_minimum_dbfs", number(-36, -96, 0)},
              {"transient_rise_db", number(12, 6, 60)},
              {"transient_refractory_ms", number(50, 10, 1000)}}}};
}
Json sourceProfile(const Json& input)
{
    need(input.is_object(), "source detector profile must be an object");
    const auto rules = sourceProfileSchema()["properties"];
    Json output = Json::object();
    for (auto i = rules.begin(); i != rules.end(); ++i)
        output[i.key()] = i.value()["default"];
    for (auto i = input.begin(); i != input.end(); ++i)
    {
        need(rules.contains(i.key()), "unknown source detector condition");
        const auto& rule = rules.at(i.key());
        need(i.value().is_number(), "source detector conditions must be numbers");
        const double value = i.value();
        need(std::isfinite(value) && value >= rule["minimum"].get<double>() && value <= rule["maximum"].get<double>(),
             "source detector condition outside finite range");
        output[i.key()] = value == 0 ? 0. : value;
    }
    return output;
}
SourceFeatures::SourceFeatures(double rate, int64_t start, const Json& requested)
    : profile(sourceProfile(requested)),
      silenceLimit(std::pow(10., profile["silence_threshold_dbfs"].get<double>() / 20)),
      transientLimit(std::pow(10., profile["transient_minimum_dbfs"].get<double>() / 20)), begin(start),
      minimumSilence(int64_t(std::ceil(rate * profile["minimum_silence_ms"].get<double>() / 1000))),
      hop(std::max(int64_t(1), int64_t(std::llround(rate * .005)))), binStart(start),
      refractory(int64_t(std::ceil(rate * profile["transient_refractory_ms"].get<double>() / 1000)))
{
}
void SourceFeatures::event(const char* kind, int64_t start, int64_t end, Json evidence)
{
    auto& count = std::string(kind) == "silence" ? silenceCount : transientCount;
    ++count;
    // Each family retains at most 64; total counts remain exact beyond the cap.
    if (count <= 64)
        events.push_back({{"kind", kind},
                          {"source_start_frame", start},
                          {"source_end_frame", end},
                          {"estimated", std::string(kind) != "silence"},
                          {"evidence", std::move(evidence)}});
}
void SourceFeatures::closeSilence(int64_t end)
{
    if (quietStart >= 0 && end - quietStart >= minimumSilence)
        event("silence", quietStart, end,
              {{"threshold_dbfs", profile["silence_threshold_dbfs"]},
               {"minimum_ms", profile["minimum_silence_ms"]},
               {"rule", "all channels abs(sample) <= threshold, contiguous native frames"}});
    quietStart = -1;
}
void SourceFeatures::closeBin(int64_t end)
{
    if (binFrames == hop)
    {
        const double energy = binEnergy / binFrames;
        double reference = 0;
        for (double v : preceding)
            reference += v / 4.;
        const double rise = 10 * std::log10(std::max(energy, 1e-20) / std::max(reference, 1e-20));
        if (bins >= 4 && firstAbove >= 0 && binPeak >= transientLimit &&
            rise >= profile["transient_rise_db"].get<double>() && firstAbove - lastOnset >= refractory)
        {
            event("transient_candidate", firstAbove, end,
                  {{"window_start_frame", binStart},
                   {"window_end_frame", end},
                   {"rms", std::sqrt(energy)},
                   {"reference_rms", std::sqrt(reference)},
                   {"rise_db", rise},
                   {"peak", binPeak},
                   {"method", "5 ms mean-square rise against preceding 20 ms; first above minimum peak"}});
            lastOnset = firstAbove;
        }
        preceding[size_t(bins % 4)] = energy;
        ++bins;
    }
    binStart = end;
    binFrames = 0;
    binEnergy = binPeak = 0;
    firstAbove = -1;
}
void SourceFeatures::frame(int64_t position, double maximum, double meanSquare)
{
    if (maximum <= silenceLimit)
    {
        if (quietStart < 0)
            quietStart = position;
    }
    else
        closeSilence(position);
    binEnergy += meanSquare;
    binPeak = std::max(binPeak, maximum);
    if (firstAbove < 0 && maximum >= transientLimit)
        firstAbove = position;
    if (++binFrames == hop)
        closeBin(position + 1);
}
Json SourceFeatures::finish(int64_t end)
{
    closeSilence(end);
    const auto text = profile.dump();
    return {{"detector", "forma-source-events/1"},
            {"profile", profile},
            {"profile_sha256", juce::SHA256(text.data(), text.size()).toHexString().toStdString()},
            {"events", events},
            {"event_counts", {{"silence", silenceCount}, {"transient_candidate", transientCount}}},
            {"events_omitted", silenceCount + transientCount - int64_t(events.size())},
            {"time_domain", "native source-file frames; half-open"},
            {"transient_hop_frames", hop},
            {"transient_warmup_frames", 4 * hop},
            {"qualifies_performance_quality", false},
            {"qualifies_breath_detection", false}};
}
} // namespace ndaw::v2::analysis
