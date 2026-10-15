// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2::project_files
{
// Non-realtime filesystem publication only. The L1 producer is called once on
// its calling message thread; this layer never holds or accesses an Edit.
Json replace(const juce::File&, const std::string& expectedHash,
             const std::function<Json(const juce::File&)>& producer);
} // namespace ndaw::v2::project_files
