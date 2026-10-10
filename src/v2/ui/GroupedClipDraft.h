#pragma once
#include "Theme.h"
#include <nativedaw/v2/ClipGroupTransform.h>
namespace ndaw::desktop
{
struct GroupedClipDraft
{
    static bool includes(const Json& drag, const std::string& id)
    {
        if (!drag.contains("linked"))
            return drag.value("clip_id", std::string{}) == id;
        for (const auto& target : drag["linked"])
            if (target["id"] == id)
                return true;
        return false;
    }
    static Json args(const Json& drag, int64_t start, int64_t end)
    {
        const auto& c = drag["clip"];
        const auto mode = drag["mode"].get<std::string>();
        if (mode == "move")
            return {{"clip", c["id"]}, {"position_samples", start}};
        if (mode == "left" || mode == "right")
            return {{"clip", c["id"]}, {"start_samples", start}, {"end_samples", end}};
        return {{"clip", c["id"]},
                {"in_samples", drag.value("preview_fade_in", c["fade_in_samples"].get<int64_t>())},
                {"out_samples", drag.value("preview_fade_out", c["fade_out_samples"].get<int64_t>())},
                {"in_curve", c["fade_in_curve"]},
                {"out_curve", c["fade_out_curve"]}};
    }
    static std::string command(const Json& drag)
    {
        const auto mode = drag["mode"].get<std::string>();
        return mode == "move" ? "clip.move" : mode == "left" || mode == "right" ? "clip.trim" : "clip.fade";
    }
    static Json clip(const Json& original, const Json& drag, int64_t start, int64_t end)
    {
        if (drag.is_null() || !drag.contains("clip") || !includes(drag, original["id"]))
            return original;
        if (drag.contains("mapped_moves") && drag["mapped_moves"].contains(original.at("id").get<std::string>()))
        {
            auto preview = original;
            preview.update(drag["mapped_moves"].at(original.at("id").get<std::string>()));
            return preview;
        }
        if (drag.contains("mapped_trims") && drag["mapped_trims"].contains(original.at("id").get<std::string>()))
        {
            auto preview = original;
            preview.update(drag["mapped_trims"].at(original.at("id").get<std::string>()));
            return preview;
        }
        const auto cmd = command(drag);
        return clipgroup::preview(original, cmd,
                                  clipgroup::relative(cmd, args(drag, start, end), drag["clip"], original,
                                                      std::llround(te::Edit::maximumLength * 48000.)));
    }
    static void constrain(const Json& facts, Json& drag, int64_t& start, int64_t& end)
    {
        if (!drag.contains("linked"))
            return;
        const auto& anchor = drag["clip"];
        const auto mode = drag["mode"].get<std::string>();
        const int64_t maximum = std::llround(te::Edit::maximumLength * 48000.);
        int64_t lower = -maximum, upper = maximum, delta = 0;
        for (const auto& track : facts["tracks"])
            for (const auto& c : track["clips"])
                if (includes(drag, c["id"]))
                {
                    const int64_t s = c["start_samples"], n = c["length_samples"],
                                  in = c.value("fade_in_samples", int64_t(0)),
                                  out = c.value("fade_out_samples", int64_t(0));
                    if (mode == "move")
                    {
                        lower = std::max(lower, -s);
                        upper = std::min(upper, maximum - s - n);
                        delta = start - anchor["start_samples"].get<int64_t>();
                    }
                    else if (mode == "left")
                    {
                        const auto available =
                            c.value("source_offset_seconds", c.value("source_offset_samples", int64_t(0)) / 48000.);
                        lower = std::max(lower, c["kind"] == "midi"
                                                    ? c["minimum_start_samples"].get<int64_t>() - s
                                                    : std::max(-s, int64_t(std::ceil(-available * 48000. - 1e-7))));
                        upper = std::min(upper, n - 1);
                        delta = start - anchor["start_samples"].get<int64_t>();
                    }
                    else if (mode == "right")
                    {
                        const auto available =
                            (c["kind"] == "midi"
                                 ? maximum / 48000.
                                 : c["source_frames"].get<double>() / c["source_sample_rate"].get<double>()) -
                            c.value("source_offset_seconds", c.value("source_offset_samples", int64_t(0)) / 48000.) -
                            n / 48000.;
                        lower = std::max(lower, 1 - n);
                        upper = std::min(
                            upper, c["kind"] == "midi"
                                       ? maximum - s - n
                                       : std::min(maximum - s - n, int64_t(std::floor(available * 48000. + 1e-7))));
                        delta = end - anchor["start_samples"].get<int64_t>() - anchor["length_samples"].get<int64_t>();
                    }
                    else
                    {
                        const bool before = mode == "fade_in";
                        lower = std::max(lower, -(before ? in : out));
                        upper = std::min(upper, n - in - out);
                        delta = drag[before ? "preview_fade_in" : "preview_fade_out"].get<int64_t>() -
                                anchor[before ? "fade_in_samples" : "fade_out_samples"].get<int64_t>();
                    }
                }
        if (upper < lower)
            throw std::runtime_error("no valid shared group edit range");
        delta = std::clamp(delta, lower, upper);
        if (mode == "move" || mode == "left")
        {
            start = anchor["start_samples"].get<int64_t>() + delta;
            end = anchor["start_samples"].get<int64_t>() + anchor["length_samples"].get<int64_t>() +
                  (mode == "move" ? delta : 0);
        }
        else if (mode == "right")
            end = anchor["start_samples"].get<int64_t>() + anchor["length_samples"].get<int64_t>() + delta;
        else
        {
            const bool before = mode == "fade_in";
            drag[before ? "preview_fade_in" : "preview_fade_out"] =
                anchor[before ? "fade_in_samples" : "fade_out_samples"].get<int64_t>() + delta;
        }
    }
};
} // namespace ndaw::desktop
