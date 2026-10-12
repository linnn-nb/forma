// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <numeric>
#include <nativedaw/v2/EngineCommands.h>

namespace ndaw::v2
{
// Opt-in message-thread measurements. Never used by the audio callback.
struct Performance
{
    inline static thread_local bool enabled = false;
    inline static thread_local std::map<std::string, std::vector<double>> samples;
    struct Span
    {
        explicit Span(const char* name) : name(name), start(enabled ? juce::Time::getMillisecondCounterHiRes() : 0) {}
        ~Span()
        {
            if (enabled)
                samples[name].push_back(juce::Time::getMillisecondCounterHiRes() - start);
        }
        const char* name;
        double start;
    };
    static Json report()
    {
        Json result = Json::object();
        for (const auto& [name, values] : samples)
        {
            auto sorted = values;
            std::sort(sorted.begin(), sorted.end());
            if (!sorted.empty())
                result[name] = {{"count", sorted.size()}, {"p95_ms", sorted[size_t(.95 * (sorted.size() - 1))]},
                                {"max_ms", sorted.back()},
                                {"total_ms", std::accumulate(sorted.begin(), sorted.end(), 0.)}};
        }
        return result;
    }
};
} // namespace ndaw::v2
