// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
class AppPreferences
{
public:
    AppPreferences(juce::File, bool legacyScrub);
    Json view() const;
    Json update(const Json&, uint64_t);
    Json history(bool redo, uint64_t);

private:
    juce::File file;
    Json document;
    std::string error, diskHash;
    void publish(Json);
};
} // namespace ndaw::v2
