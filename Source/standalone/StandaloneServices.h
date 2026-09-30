// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

namespace p5x::standalone
{
// What the Standalone app offers the shared UI (settings dialog). In the VST3 there is no
// implementation and getServices() returns nullptr, so the dialog shows only the MIDI and HQ items.
class Services
{
public:
    virtual ~Services() = default;

    virtual juce::AudioDeviceManager& getDeviceManager() = 0;
    virtual void resetAudio() = 0; // close and reopen the device (also re-runs prepareToPlay)
    virtual void setInputMuted (bool muted) = 0; // the holder passes input only while routing is on
    virtual bool isComputerKeyboardEnabled() const = 0;
    virtual void setComputerKeyboardEnabled (bool enabled) = 0;
};

Services* getServices() noexcept;
void setServices (Services* services) noexcept;
} // namespace p5x::standalone
