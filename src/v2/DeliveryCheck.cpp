#include <nativedaw/v2/DeliveryCheck.h>

namespace ndaw::v2::delivery
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
Json number(double low, double high, double value, const char* unit)
{
    return {{"type", "number"}, {"minimum", low}, {"maximum", high}, {"default", value}, {"description", unit}};
}
bool finite(const Json& value)
{
    return value.is_number() && std::isfinite(value.get<double>());
}
} // namespace
Json profileSchema()
{
    return {
        {"type", "object"},
        {"properties",
         {{"target_lufs", number(-70, 0, -14, "Target LUFS-I. Example profile only; not a platform certification.")},
          {"lufs_tolerance", number(0, 6, 1, "Allowed absolute LUFS-I difference, in LU.")},
          {"true_peak_ceiling_dbtp", number(-20, 0, -1, "Maximum measured True Peak, in dBTP (inclusive).")},
          {"expect_silent_ending",
           {{"type", "boolean"},
            {"default", true},
            {"description",
             "Review active signal in the last 100 ms; this cannot certify absence of truncated effect tails."}}},
          {"ending_peak_ceiling_dbfs",
           number(-120, 0, -60,
                  "Last-window sample peak ceiling, in dBFS. Samples outside the selected range are not measured.")}}},
        {"additionalProperties", false},
        {"default",
         {{"target_lufs", -14},
          {"lufs_tolerance", 1},
          {"true_peak_ceiling_dbtp", -1},
          {"expect_silent_ending", true},
          {"ending_peak_ceiling_dbfs", -60}}}};
}
Json normaliseProfile(const Json& input)
{
    require(input.is_object(), "delivery profile must be an object");
    auto schema = profileSchema();
    auto output = schema["default"];
    for (auto i = input.begin(); i != input.end(); ++i)
    {
        require(schema["properties"].contains(i.key()), "unknown delivery profile field");
        const auto& rule = schema["properties"][i.key()];
        if (rule["type"] == "boolean")
            require(i.value().is_boolean(), "ending policy must be a boolean");
        else
            require(finite(i.value()) && i.value().get<double>() >= rule["minimum"].get<double>() &&
                        i.value().get<double>() <= rule["maximum"].get<double>(),
                    "delivery threshold outside supported finite range");
        output[i.key()] = i.value();
    }
    // Canonicalise numeric representation too: JSON 1 and 1.0 express the same
    // finite condition. Omitted defaults and explicitly supplied defaults must
    // produce the same profile/request SHA256, independent of the client.
    for (auto i = output.begin(); i != output.end(); ++i)
        if (i.value().is_number())
        {
            const auto value = i.value().get<double>();
            i.value() = value == 0 ? 0. : value;
        }
    return output;
}
Json evaluate(const Json& measured, const Json& requested)
{
    const auto profile = normaliseProfile(requested);
    require(measured.is_object() && measured.value("audio_verified", Json(false)) == true,
            "delivery check requires verified audio measurement");
    require(measured.contains("frames") && measured["frames"].is_number_integer() &&
                measured["frames"].get<int64_t>() > 0,
            "delivery check requires actual nonempty audio");
    Json checks = Json::array();
    auto add = [&](const char* id, const char* label, const char* status, Json actual, Json limits, const char* why)
    {
        checks.push_back({{"id", id},
                          {"label", label},
                          {"status", status},
                          {"actual", std::move(actual)},
                          {"limits", std::move(limits)},
                          {"explanation", why}});
    };
    const auto loudness = measured.value("lufs_i", Json(nullptr));
    const double target = profile["target_lufs"], tolerance = profile["lufs_tolerance"];
    add("integrated_loudness", "整体响度",
        finite(loudness) ? std::abs(loudness.get<double>() - target) <= tolerance ? "passed" : "failed"
                         : "indeterminate",
        loudness,
        {{"target_lufs", target},
         {"tolerance_lu", tolerance},
         {"minimum_lufs", target - tolerance},
         {"maximum_lufs", target + tolerance}},
        finite(loudness) ? "Measured LUFS-I is compared to the explicit target and tolerance."
                         : "Silence or an insufficient interval cannot supply a valid integrated loudness value.");
    const auto peak = measured.value("peak", Json(nullptr)), truePeak = measured.value("true_peak_dbtp", Json(nullptr));
    const double ceiling = profile["true_peak_ceiling_dbtp"];
    const bool silent = finite(peak) && peak.get<double>() == 0;
    add("true_peak", "True Peak",
        finite(truePeak) ? truePeak.get<double>() <= ceiling ? "passed" : "failed"
        : silent         ? "passed"
                         : "indeterminate",
        truePeak, {{"ceiling_dbtp", ceiling}},
        silent ? "Verified digital silence has no finite dBTP level (negative infinity)."
               : "Measured interpolated True Peak, not sample peak, is compared to the explicit ceiling.");
    const auto over = measured.value("over_full_scale_frames", Json(nullptr));
    const bool count = over.is_number_integer() && over.get<double>() >= 0;
    add("full_scale", "超过满刻度", count ? over.get<int64_t>() == 0 ? "passed" : "failed" : "indeterminate", over,
        {{"maximum_frames", 0}, {"sample_threshold", 1.0}},
        "Contiguous abs(sample)>=1 indicates integer-export clipping risk, not proof of previously damaged media.");
    const auto end = measured.value("ending_window", Json(nullptr));
    const bool fullWindow = end.is_object() && finite(end.value("duration_ms", Json(nullptr))) &&
                            end["duration_ms"].get<double>() >= 100. - 1e-6;
    const auto endPeak = end.is_object() ? end.value("peak", Json(nullptr)) : Json(nullptr);
    const double endCeiling = profile["ending_peak_ceiling_dbfs"];
    const bool enabled = profile["expect_silent_ending"];
    const char* endStatus = !enabled                                                   ? "not_required"
                            : !fullWindow || !finite(endPeak)                          ? "indeterminate"
                            : endPeak.get<double>() <= std::pow(10., endCeiling / 20.) ? "passed"
                                                                                       : "review";
    add("ending_level", "末尾电平 / 截断风险", endStatus, end,
        {{"expect_silent_ending", enabled}, {"window_ms", 100}, {"ceiling_dbfs", endCeiling}},
        "Only the selected range's last 100 ms is measured. Active signal needs a human decision. Quiet ending cannot "
        "prove all reverb/delay tails were included; no look-ahead or effect-tail simulation is performed.");
    std::string status = "passed";
    for (const auto& check : checks)
        if (check["status"] == "review")
            status = "needs_review";
    for (const auto& check : checks)
        if (check["status"] == "indeterminate")
            status = "indeterminate";
    for (const auto& check : checks)
        if (check["status"] == "failed")
            status = "failed";
    const auto profileText = profile.dump();
    return {{"checker", "forma-delivery/1"},
            {"scope", "offline_master_range_criteria"},
            {"status", status},
            {"profile", profile},
            {"profile_sha256", juce::SHA256(profileText.data(), profileText.size()).toHexString().toStdString()},
            {"checks", checks},
            {"tail_truncation_certified", false},
            {"export_file_certified", false},
            {"platform_certified", false}};
}
} // namespace ndaw::v2::delivery
