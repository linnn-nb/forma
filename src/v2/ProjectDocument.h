// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <map>
#include <optional>
#include <string>

namespace ndaw::v2::project_document
{
using Requirements = std::map<std::string, int>;
struct Document
{
    juce::ValueTree edit;
    std::optional<Requirements> declared;
    // Old schema1 writers understood print audio but omitted these two IDs.
    // Original payload/history integrity is still checked before adoption.
    bool legacyPrintRequirements = false;
};
const Requirements& supported();
// Conservative closure over native state and unpacked history deltas. Opaque
// plugin properties are data, never interpreted as project capability metadata.
void collect(juce::ValueTree, Requirements&);
void validate(const Requirements& actual, const std::optional<Requirements>& declared = {}, bool legacyPrint = false);
std::unique_ptr<juce::XmlElement> write(juce::ValueTree, const Requirements&);
Document read(const juce::XmlElement&);
} // namespace ndaw::v2::project_document
