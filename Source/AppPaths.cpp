// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "AppPaths.h"

namespace p5x::paths
{
juce::File appDataDir()
{
    const auto overrideDir = juce::SystemStats::getEnvironmentVariable ("P5X_DATA_DIR", {});

    if (overrideDir.isNotEmpty() && juce::File::isAbsolutePath (overrideDir))
        return juce::File (overrideDir);

    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("P5X");
}
} // namespace p5x::paths
