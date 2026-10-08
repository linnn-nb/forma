#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// Read-only UI policy. L1 independently validates current inputs and the whole Plan at commit.
struct TrackRecordingState
{
    static bool supported(const Json& track)
    {
        if (!track.is_object())
            return false;
        const auto type = track.value("type", std::string{});
        return (type == "audio" || type == "midi" || type == "instrument") && track.contains("input") &&
               track["input"].is_object();
    }
    static bool blocked(const Json& session)
    {
        if (!session.is_object())
            return true;
        const auto audio = session.value("audio_configuration", Json(nullptr));
        const auto midi = session.value("midi_configuration", Json(nullptr));
        return session.value("playing", false) || session.value("recording", false) ||
               !session.value("recording_capture", Json(nullptr)).is_null() ||
               !session.value("automation_capture", Json(nullptr)).is_null() ||
               !session.value("parameter_capture", Json(nullptr)).is_null() ||
               (audio.is_object() && audio.value("state", std::string{}) == "preparing") ||
               (midi.is_object() && midi.value("state", std::string{}) == "requested");
    }
    static std::string toggleMonitor(const Json& input)
    {
        // Keep Off explicit. Auto is armed monitoring in this engine, not PT Punch switching.
        return input.value("monitor", std::string("off")) == "on" ||
                       (!input.value("available", false) && input.value("monitor", std::string("off")) != "off")
                   ? "off"
                   : "on";
    }
    static bool canArm(const Json& track, bool busy)
    {
        return supported(track) && !busy &&
               (track["input"].value("available", false) || track["input"].value("armed", false));
    }
    static bool canMonitor(const Json& track, bool busy)
    {
        return supported(track) && !busy &&
               (track["input"].value("available", false) ||
                track["input"].value("monitor", std::string("off")) != "off");
    }
};
class InputMonitorButton final : public juce::TextButton
{
public:
    InputMonitorButton() : juce::TextButton("I") {}
    std::function<void()> onMenu;
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (onMenu)
                onMenu();
            return;
        }
        juce::TextButton::mouseDown(e);
    }
};
} // namespace ndaw::desktop
