// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// A layout-derived base key is a fallback. Existing literal bindings, including
// user-captured symbols, always retain priority over a physical/default chord.
inline juce::KeyPress resolveShortcutKey(const juce::KeyPress& raw, const juce::KeyPressMappingSet& mappings,
                                         int baseKey)
{
    if (mappings.findCommandForKeyPress(raw) != 0 || baseKey == 0)
        return raw;
    return juce::KeyPress(baseKey, raw.getModifiers(), 0);
}
#if JUCE_MAC
int nativeShortcutBaseKey(const juce::KeyPress&);
#endif
inline juce::KeyPress shortcutKey(const juce::KeyPress& raw, const juce::KeyPressMappingSet& mappings)
{
#if JUCE_MAC
    return resolveShortcutKey(raw, mappings, nativeShortcutBaseKey(raw));
#else
    return raw;
#endif
}
} // namespace ndaw::desktop
