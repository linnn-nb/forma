// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <tracktion_engine/tracktion_engine.h>
namespace ndaw::v2::detail
{
// Drain only native derived ordering, never the message queue.
// Sorting must belong to the originating Plan before JUCE Undo/Redo closes it.
inline void settleClipOrder(tracktion::engine::Edit& edit)
{
    namespace te = tracktion::engine;
    edit.getTrackList().flushPendingOrder();
    for (auto* track : te::getAllTracks(edit))
        if (auto* children = track->getSubTrackList())
            children->flushPendingOrder();
    const auto settle = [](auto&& self, te::ClipOwner& owner) -> void
    {
        owner.flushPendingClipOrder();
        // The SDK sort emits child-order notifications which schedule one more
        // sort. Drain that echo while the originating transaction is still open;
        // otherwise it can run after Undo and discard the native Redo branch.
        owner.flushPendingClipOrder();
        for (auto* clip : owner.getClips())
            if (auto* nested = dynamic_cast<te::ClipOwner*>(clip))
                self(self, *nested);
    };
    for (auto* track : te::getAllTracks(edit))
        if (auto* owner = dynamic_cast<te::ClipOwner*>(track))
            settle(settle, *owner);
}
} // namespace ndaw::v2::detail
