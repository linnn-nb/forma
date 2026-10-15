// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2::midi_loop
{
std::string fingerprint(juce::ValueTree);
void validate(te::Edit&);
Json summary(te::Edit&, juce::ValueTree);
} // namespace ndaw::v2::midi_loop
