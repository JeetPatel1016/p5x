// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <juce_core/juce_core.h>

namespace p5x::paths
{
// %APPDATA%/P5X (09-presets-and-state.md § What lives where). Tests point this elsewhere by setting
// the P5X_DATA_DIR environment variable, so they never touch the user's real files.
juce::File appDataDir();

inline juce::File logsDir() { return appDataDir().getChildFile ("logs"); }
inline juce::File settingsFile() { return appDataDir().getChildFile ("settings.xml"); }
inline juce::File midiMapFile() { return appDataDir().getChildFile ("midi-map.xml"); }
inline juce::File standaloneFile() { return appDataDir().getChildFile ("standalone.xml"); }
} // namespace p5x::paths
