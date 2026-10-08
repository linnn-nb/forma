#pragma once
#include <nativedaw/v2/EngineCommands.h>

namespace ndaw::v2::analysis {
// Detached evidence only. Exclusive ends locate at the last decoded frame.
Json loudnessPoint(const Json& curve, int64_t index, const std::string& series);
}
