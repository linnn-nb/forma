// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <json.hpp>
namespace ndaw::v2::warp_onsets
{
using Json = nlohmann::json;
// Detached facts only. These helpers never receive or modify a live Edit.
Json read(juce::ValueTree evidence, const std::string& mediaHash);
Json candidates(const Json& bound, const Json& anchors, const Json& receipt);
Json insert(const Json& bound, Json anchors, const Json& selectedCandidates);
Json append(const Json& records, const Json& bound, const Json& receipt);
Json views(const Json& records, const Json& fullAnchors, const std::string& clip, int64_t start, int64_t begin,
           int64_t length);
std::string digest(const Json&);
} // namespace ndaw::v2::warp_onsets
