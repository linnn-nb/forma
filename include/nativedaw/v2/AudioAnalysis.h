#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include <atomic>

namespace ndaw::v2::analysis {
// Detached PCM only: never receives an Edit, device or mutable plugin.
struct Control {
    std::function<bool()> cancelled = [] { return false; };
    std::function<void()> yield = [] {};
};
Json measure(const juce::File&, int64_t sessionStart, const Control& = {});
}
