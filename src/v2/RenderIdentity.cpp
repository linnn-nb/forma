// SPDX-License-Identifier: AGPL-3.0-only
#include "RenderIdentity.h"
#include "RenderBuild.h"
#include <juce_cryptography/juce_cryptography.h>
namespace ndaw::v2
{
Json renderImplementationIdentity()
{
    return {{"schema", "forma-render/1"},
            {"implementation_sha256", FORMA_RENDER_IMPLEMENTATION_SHA},
            {"tracktion_commit", FORMA_RENDER_TE_COMMIT},
            {"tracktion_patches_sha256", FORMA_RENDER_TE_PATCH_SHA},
            {"juce_commit", "37c894f83d379179b2070d437ccd0f1cd9af9576"},
            {"compiler", FORMA_RENDER_COMPILER},
            {"target", FORMA_RENDER_TARGET},
#if JUCE_DEBUG
            {"configuration", "Debug"},
#else
            {"configuration", "Release"},
#endif
            {"scope", "conservative L1/L0 wrapper sources, SDK pins and audited patches; plugin module hashes "
                      "and opaque state belong to Edit; private plugin assets are not independently hashed"}};
}
Json renderBinding(const std::string& editXml)
{
    Json binding = {{"render_implementation", renderImplementationIdentity()},
                    {"edit_snapshot_sha256", juce::SHA256(editXml.data(), editXml.size()).toHexString().toStdString()}};
    const auto encoded = binding.dump();
    binding["processing_chain_hash"] = juce::SHA256(encoded.data(), encoded.size()).toHexString().toStdString();
    return binding;
}
} // namespace ndaw::v2
