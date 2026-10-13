// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
// Only for new, test-owned XML derivatives. A manually changed Edit cannot claim
// that its old persistent Undo archive still describes the current state. Real
// save/reopen/Undo tests keep the archive; corruption tests deliberately retain it.
inline void discardDerivedFixtureHistory(juce::XmlElement& edit)
{
    if (!edit.hasTagName("EDIT"))
        throw std::runtime_error("derived test fixture requires native Edit XML");
    if (auto* metadata = edit.getChildByName("NATIVEDAW"))
        if (auto* history = metadata->getChildByName("PERSISTENT_HISTORY"))
            metadata->removeChildElement(history, true);
}
