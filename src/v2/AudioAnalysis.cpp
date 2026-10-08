#include <nativedaw/v2/AudioAnalysis.h>
#include <nativedaw/v2/SourceFeatures.h>
#include <nativedaw/v2/Spectrum.h>
#include <ebur128.h>

namespace ndaw::v2::analysis
{
namespace
{
void require(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}
Json db(double value)
{
    return value > 0 ? Json(20 * std::log10(value)) : Json(nullptr);
}
struct Sum
{
    double value = 0, error = 0;
    void add(double x)
    {
        const double y = x - error, next = value + y;
        error = (next - value) - y;
        value = next;
    }
};
void loudnessNumber(double value)
{
    require(std::isfinite(value) || value == -INFINITY, "nonfinite loudness result");
}
} // namespace
Json measure(const juce::File& source, int64_t start, const Control& control, FrameRange range, const Json& features,
             FeatureDomain domain)
{
    // Render workers and callers may inherit different FTZ/DAZ states. Make
    // subnormal handling explicit and restore the caller's state on exit.
    const juce::ScopedNoDenormals noDenormals;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    require(reader && reader->numChannels >= 1 && reader->numChannels <= 2 && reader->lengthInSamples > 0,
            "analysis requires nonempty mono/stereo audio");
    require(start >= 0 && reader->sampleRate >= 8000 && reader->sampleRate <= 192000, "invalid analysis time domain");
    if (range.end == -1)
        range.end = reader->lengthInSamples;
    require(range.begin >= 0 && range.end > range.begin && range.end <= reader->lengthInSamples,
            "analysis source frame range outside media");
    const auto length = range.end - range.begin;
    require(start <= 9007199254740991LL && length * 48000. / reader->sampleRate <= 9007199254740991LL - start,
            "analysis positions exceed exact session sample domain");
    std::unique_ptr<SourceFeatures> detector;
    if (!features.is_null())
        detector = std::make_unique<SourceFeatures>(reader->sampleRate, range.begin, features);
    auto* meter = ebur128_init(reader->numChannels, static_cast<unsigned long>(reader->sampleRate),
                               EBUR128_MODE_I | EBUR128_MODE_S | EBUR128_MODE_M | EBUR128_MODE_TRUE_PEAK);
    require(meter, "libebur128 initialisation failed");
    struct Cleanup
    {
        ebur128_state* meter;
        ~Cleanup()
        {
            ebur128_destroy(&meter);
        }
    } cleanup{meter};
    const int blockFrames = int((static_cast<unsigned long>(reader->sampleRate) + 5) / 10);
    require(length <= reader->sampleRate * 300, "analysis exceeds 300 second curve budget");
    Spectrum spectrum(reader->sampleRate, int(reader->numChannels), range.begin, start, domain, control);
    juce::AudioBuffer<float> buffer(reader->numChannels, blockFrames);
    std::vector<float> interleaved(size_t(blockFrames) * reader->numChannels);
    double peak = 0, endingPeak = 0;
    Sum sum, sumL, sumR, LL, RR, LR, endingSum;
    const auto endingFrames = std::min(length, int64_t(std::llround(reader->sampleRate * .1)));
    const auto endingStart = length - endingFrames;
    int64_t peakFrame = 0, overFrames = 0, eventStart = -1, eventCount = 0, windowM = 0, windowS = 0;
    double maxM = -INFINITY, maxS = -INFINITY;
    Json events = Json::array(), curvePoints = Json::array();
    auto curveValue = [](double level)
    { return std::isfinite(level) ? Json(std::round(level * 1e6) / 1e6) : Json(nullptr); };
    auto position = [&](int64_t frame) { return start + std::llround(frame * 48000. / reader->sampleRate); };
    auto closeEvent = [&](int64_t end)
    {
        if (eventStart < 0)
            return;
        ++eventCount;
        if (events.size() < 128)
            events.push_back({{"id", events.size()},
                              {"kind", "full_scale_exceedance"},
                              {"start_samples", position(eventStart)},
                              {"end_samples", position(end)},
                              {"render_start_frame", eventStart},
                              {"render_end_frame", end}});
        eventStart = -1;
    };
    for (int64_t offset = 0; offset < length; offset += blockFrames)
    {
        require(!control.cancelled(), "analysis cancelled or deadline expired");
        control.yield();
        const int n = int(std::min<int64_t>(blockFrames, length - offset));
        require(reader->read(&buffer, 0, n, range.begin + offset, true, true), "analysis PCM read failed");
        for (int i = 0; i < n; ++i)
        {
            bool over = false;
            double framePeak = 0, frameSquare = 0;
            for (unsigned ch = 0; ch < reader->numChannels; ++ch)
            {
                const double v = buffer.getSample(ch, i);
                require(std::isfinite(v), "nonfinite analysis sample");
                interleaved[size_t(i) * reader->numChannels + ch] = float(v);
                sum.add(v * v);
                if (detector)
                {
                    framePeak = std::max(framePeak, std::abs(v));
                    frameSquare += v * v;
                }
                if (offset + i >= endingStart)
                {
                    endingSum.add(v * v);
                    endingPeak = std::max(endingPeak, std::abs(v));
                }
                if (std::abs(v) > peak)
                {
                    peak = std::abs(v);
                    peakFrame = offset + i;
                }
                over |= std::abs(v) >= 1.;
            }
            spectrum.frame(interleaved.data() + size_t(i) * reader->numChannels);
            if (detector)
                detector->frame(range.begin + offset + i, framePeak, frameSquare / reader->numChannels);
            if (over)
            {
                ++overFrames;
                if (eventStart < 0)
                    eventStart = offset + i;
            }
            else
                closeEvent(offset + i);
            if (reader->numChannels == 2)
            {
                const double l = buffer.getSample(0, i), r = buffer.getSample(1, i);
                sumL.add(l);
                sumR.add(r);
                LL.add(l * l);
                RR.add(r * r);
                LR.add(l * r);
            }
        }
        require(ebur128_add_frames_float(meter, interleaved.data(), size_t(n)) == EBUR128_SUCCESS,
                "loudness processing failed");
        const auto measured = offset + n;
        if (n == blockFrames && measured >= 4 * blockFrames)
        {
            double m, s = -INFINITY;
            require(ebur128_loudness_momentary(meter, &m) == EBUR128_SUCCESS, "momentary loudness failed");
            loudnessNumber(m);
            ++windowM;
            maxM = std::max(maxM, m);
            if (measured >= 30 * blockFrames)
            {
                require(ebur128_loudness_shortterm(meter, &s) == EBUR128_SUCCESS, "short-term loudness failed");
                loudnessNumber(s);
                ++windowS;
                maxS = std::max(maxS, s);
            }
            require(curvePoints.size() < 3000, "loudness curve exceeds 3000 point budget");
            curvePoints.push_back(Json::array({measured, curveValue(m), curveValue(s)}));
        }
    }
    closeEvent(length);
    double integrated = -INFINITY, truePeak = 0;
    if (length >= std::llround(reader->sampleRate * .4))
        require(ebur128_loudness_global(meter, &integrated) == EBUR128_SUCCESS, "integrated loudness failed");
    loudnessNumber(integrated);
    for (unsigned ch = 0; ch < reader->numChannels; ++ch)
    {
        double p;
        require(ebur128_true_peak(meter, ch, &p) == EBUR128_SUCCESS && std::isfinite(p) && p >= 0,
                "true peak failed or nonfinite");
        truePeak = std::max(truePeak, p);
    }
    Json correlation = nullptr;
    const double frames = double(length),
                 denominator = std::sqrt(std::max(0., LL.value - sumL.value * sumL.value / frames) *
                                         std::max(0., RR.value - sumR.value * sumR.value / frames));
    if (reader->numChannels == 2 && denominator > 1e-20)
        correlation = std::clamp((LR.value - sumL.value * sumR.value / frames) / denominator, -1., 1.);
    const double rms = std::sqrt(sum.value / (frames * reader->numChannels)),
                 endingRms = std::sqrt(endingSum.value / (endingFrames * reader->numChannels));
    Json curve = {{"version", "forma-loudness-curve/1"},
                  {"columns", {"decoded_end_frame", "momentary_lufs", "short_term_lufs"}},
                  {"points", std::move(curvePoints)},
                  {"point_count", windowM},
                  {"maximum_points", 3000},
                  {"value_resolution_lu", 1e-6},
                  {"time_domain",
                   domain == FeatureDomain::SourceFrames ? "native source-file frames" : "session samples at 48000 Hz"},
                  {"sample_rate", reader->sampleRate},
                  {"decoded_start_frame", range.begin},
                  {"session_start_samples", domain == FeatureDomain::SessionSamples ? Json(start) : Json(nullptr)},
                  {"hop_frames", blockFrames},
                  {"momentary_window_frames", 4 * blockFrames},
                  {"short_term_window_frames", 30 * blockFrames},
                  {"frames", length},
                  {"trailing_frames", length % blockFrames},
                  {"null_rule", "end < window: insufficient_window; otherwise negative_infinity; never zero LUFS"},
                  {"history_scope", "K-weighting state starts at the selected decode range; no earlier audio history"}};
    require(curve.dump().size() <= 192 * 1024, "loudness curve exceeds 192 KiB budget");
    Json result = {
        {"analyser", "forma-pcm/6 + libebur128/1.2.6"},
        {"audio_verified", true},
        {"spectrum", spectrum.finish()},
        {"loudness_curve", std::move(curve)},
        {"frames", length},
        {"sample_rate", reader->sampleRate},
        {"channels", reader->numChannels},
        {"file_bits", reader->bitsPerSample},
        {"file_float", reader->usesFloatingPointData},
        {"read_range", {{"begin_frame", range.begin}, {"end_frame", range.end}}},
        {"peak_file_frame", range.begin + peakFrame},
        {"peak", peak},
        {"peak_dbfs", db(peak)},
        {"peak_position_samples", position(peakFrame)},
        {"rms", rms},
        {"rms_dbfs", db(rms)},
        {"lufs_i", std::isfinite(integrated) ? Json(integrated) : Json(nullptr)},
        {"lufs_m_max", std::isfinite(maxM) ? Json(maxM) : Json(nullptr)},
        {"lufs_s_max", std::isfinite(maxS) ? Json(maxS) : Json(nullptr)},
        {"loudness_windows",
         {{"hop_ms", 100},
          {"momentary_ms", 400},
          {"short_term_ms", 3000},
          {"momentary_count", windowM},
          {"short_term_count", windowS}}},
        {"true_peak_dbtp", db(truePeak)},
        {"correlation", correlation},
        {"over_full_scale_frames", overFrames},
        {"events", std::move(events)},
        {"event_count", eventCount},
        {"events_omitted", std::max(int64_t(0), eventCount - 128)},
        {"ending_window",
         {{"requested_ms", 100},
          {"frames", endingFrames},
          {"duration_ms", endingFrames * 1000. / reader->sampleRate},
          {"start_samples", position(endingStart)},
          {"end_samples", position(length)},
          {"peak", endingPeak},
          {"peak_dbfs", db(endingPeak)},
          {"rms", endingRms},
          {"rms_dbfs", db(endingRms)}}},
        {"parameters",
         {{"denormal_policy",
           "flush subnormal floating-point intermediates to zero; ScopedNoDenormals restores caller state"},
          {"full_scale_threshold", 1.0},
          {"event_rule", "contiguous frames with abs(sample) >= 1 in any channel; half-open session interval"},
          {"meaning", "full-scale exceedance / integer-export clipping risk, not proof of previously clipped media"}}}};
    if (detector)
    {
        auto detected = detector->finish(range.end);
        if (domain == FeatureDomain::SourceFrames)
            result["source_features"] = std::move(detected);
        else
        {
            auto merged = result["events"];
            for (auto& event : merged)
                event["estimated"] = false;
            // These coordinates refer to the file just decoded (the render),
            // never to original media. Preserve the integer render frame and
            // map it using the actual rate and selected session origin.
            for (auto event : detected["events"])
            {
                const auto first = event.at("source_start_frame").get<int64_t>() - range.begin,
                           last = event.at("source_end_frame").get<int64_t>() - range.begin;
                event.erase("source_start_frame");
                event.erase("source_end_frame");
                event["render_start_frame"] = first;
                event["render_end_frame"] = last;
                event["start_samples"] = position(first);
                event["end_samples"] = position(last);
                auto& evidence = event["evidence"];
                if (evidence.contains("window_start_frame"))
                {
                    const auto begin = evidence["window_start_frame"].get<int64_t>() - range.begin,
                               end = evidence["window_end_frame"].get<int64_t>() - range.begin;
                    evidence["window_start_frame"] = begin;
                    evidence["window_end_frame"] = end;
                    evidence["window_start_samples"] = position(begin);
                    evidence["window_end_samples"] = position(end);
                }
                if (evidence.contains("rule"))
                    evidence["rule"] = "all decoded render channels abs(sample) <= threshold, contiguous render frames";
                evidence["audio_origin"] = "decoded PCM at the requested tap; not original source evidence";
                merged.push_back(std::move(event));
            }
            std::stable_sort(
                merged.begin(), merged.end(), [](const auto& a, const auto& b)
                { return a["start_samples"].template get<int64_t>() < b["start_samples"].template get<int64_t>(); });
            const auto& counts = detected["event_counts"];
            result["event_counts"] = {{"full_scale_exceedance", eventCount},
                                      {"silence", counts["silence"]},
                                      {"transient_candidate", counts["transient_candidate"]}};
            result["event_count"] =
                eventCount + counts["silence"].get<int64_t>() + counts["transient_candidate"].get<int64_t>();
            if (merged.size() > 128)
                merged.erase(merged.begin() + 128, merged.end());
            for (size_t i = 0; i < merged.size(); ++i)
                merged[i]["id"] = i;
            result["events"] = std::move(merged);
            result["events_omitted"] = result["event_count"].get<int64_t>() - int64_t(result["events"].size());
            result["event_time_domain"] = "session samples at 48000 Hz; half-open";
            detected.erase("events");
            detected["family_candidates_omitted"] = detected["events_omitted"];
            detected.erase("events_omitted");
            detected["detector"] = "forma-render-events/1";
            detected["time_domain"] = "session samples at 48000 Hz; render frames relative to selected range";
            detected["audio_origin"] = "decoded PCM at the requested tap";
            result["processed_features"] = std::move(detected);
        }
    }
    return result;
}
} // namespace ndaw::v2::analysis
