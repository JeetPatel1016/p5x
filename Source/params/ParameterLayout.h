// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace p5x::params
{
// The complete parameter set from 01-parameters.md, grouped by panel section. The order of
// parameters matches kAllIds in ParameterIDs.h.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace p5x::params
