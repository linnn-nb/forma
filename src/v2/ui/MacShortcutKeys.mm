// SPDX-License-Identifier: AGPL-3.0-only
#include "ShortcutKeys.h"
#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

namespace ndaw::desktop
{
int nativeShortcutBaseKey(const juce::KeyPress& raw)
{
    // Only a current native key event supplies a hardware/layout identity.
    // Synthetic JUCE presses and special/keypad keys retain their literal code.
    NSEvent* event = [NSApp currentEvent];
    if (!raw.getModifiers().isShiftDown() || raw.getKeyCode() < 32 || raw.getKeyCode() > 0xff || !event ||
        [event type] != NSEventTypeKeyDown)
        return 0;
    const auto flags = [event modifierFlags];
    const int modifiers = ((flags & NSEventModifierFlagShift) ? juce::ModifierKeys::shiftModifier : 0) |
                          ((flags & NSEventModifierFlagControl) ? juce::ModifierKeys::ctrlModifier : 0) |
                          ((flags & NSEventModifierFlagOption) ? juce::ModifierKeys::altModifier : 0) |
                          ((flags & NSEventModifierFlagCommand) ? juce::ModifierKeys::commandModifier : 0);
    if (raw.getModifiers().withoutMouseButtons().getRawFlags() != modifiers)
        return 0;
    // Keyboard-layout source stays available when the selected input method is
    // Pinyin. Unlike character substitution, translation also respects non-US layouts.
    TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
    if (!source)
        return 0;
    auto* data = static_cast<CFDataRef>(TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
    if (!data)
    {
        CFRelease(source);
        return 0;
    }
    const auto* layout = reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(data));
    UInt32 dead = 0;
    UniChar chars[4]{};
    UniCharCount count = 0;
    const auto status = UCKeyTranslate(layout, [event keyCode], kUCKeyActionDown, 0, LMGetKbdType(),
                                       kUCKeyTranslateNoDeadKeysMask, &dead, 4, &count, chars);
    CFRelease(source);
    return status == noErr && count == 1 && chars[0] >= 32 ? int(chars[0]) : 0;
}
} // namespace ndaw::desktop
