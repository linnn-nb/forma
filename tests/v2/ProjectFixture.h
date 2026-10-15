// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../../src/v2/ProjectDocument.h"
#include "../../src/v2/NativeEditXml.h"
namespace ndaw::v2::project_fixture
{
// Read real on-disk project data through the production decoder before tests
// inspect native XML. Mutation fixtures must still explicitly remove history
// or intentionally assert its integrity rejection; this helper never repairs it.
inline std::unique_ptr<juce::XmlElement> nativeXml(const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (!xml)
        throw std::runtime_error("owned project fixture XML cannot be parsed");
    return preciseEditXml(project_document::read(*xml).edit);
}
} // namespace ndaw::v2::project_fixture
