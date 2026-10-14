// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_cryptography/juce_cryptography.h>
namespace ndaw::v2::clip_reference
{
inline bool hasArrangementClips(te::Edit& edit)
{
    for (auto* track : te::getAudioTracks(edit))
        if (track->getClips().size() != 0)
            return true;
    return false;
}
// A reference fingerprint is not a media-content hash. Revision/native-state
// capture protects processors; this binds the object, owner, source and extent.
inline std::string fingerprint(te::WaveAudioClip& clip)
{
    const Json reference{{"clip", clip.itemID.toString().toStdString()},
                         {"track", clip.getTrack()->itemID.toString().toStdString()},
                         {"source", clip.state[te::IDs::source].toString().toStdString()},
                         {"resolved_path", clip.getOriginalFile().getFullPathName().toStdString()},
                         {"start_seconds", double(clip.state[te::IDs::start])},
                         {"length_seconds", double(clip.state[te::IDs::length])},
                         {"offset_seconds", double(clip.state[te::IDs::offset])}};
    const auto text = reference.dump();
    return juce::SHA256(text.data(), text.size()).toHexString().toStdString();
}
} // namespace ndaw::v2::clip_reference
