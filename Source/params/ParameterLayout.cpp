// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "params/ParameterLayout.h"
#include "params/ParameterIDs.h"

#include <cmath>

namespace p5x::params
{
namespace
{
constexpr int kVersionHint = 1; // 01-parameters.md § Conventions

using Group = juce::AudioProcessorParameterGroup;
using FloatRange = juce::NormalisableRange<float>;

juce::String threeSignificant (float value)
{
    const float magnitude = std::abs (value);

    if (magnitude < 1.0e-6f)
        return "0";

    const int decimals = juce::jlimit (0, 4, 2 - (int) std::floor (std::log10 (magnitude)));
    return juce::String (value, decimals);
}

juce::String signedInt (int value)
{
    return value > 0 ? "+" + juce::String (value) : juce::String (value);
}

FloatRange linearRange (float min, float max) { return { min, max }; }

// "Log" = the knob midpoint is the geometric mean of the range.
FloatRange logRange (float min, float max)
{
    FloatRange range (min, max);
    range.setSkewForCentre (std::sqrt (min * max));
    return range;
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name, FloatRange range,
                                                       float defaultValue, const char* label)
{
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (label)
                          .withStringFromValueFunction ([] (float v, int) { return threeSignificant (v); })
                          .withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue(); });

    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, kVersionHint }, name, range,
                                                        defaultValue, attributes);
}

std::unique_ptr<juce::AudioParameterInt> makeSemitones (const char* id, const juce::String& name, int min, int max,
                                                         int defaultValue)
{
    auto attributes = juce::AudioParameterIntAttributes()
                          .withLabel ("st")
                          .withStringFromValueFunction ([] (int v, int) { return signedInt (v); })
                          .withValueFromStringFunction ([] (const juce::String& text) { return text.getIntValue(); });

    return std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, kVersionHint }, name, min, max,
                                                      defaultValue, attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const juce::String& name, bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, kVersionHint }, name, defaultValue);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const char* id, const juce::String& name,
                                                         const juce::StringArray& options, int defaultIndex)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, kVersionHint }, name, options,
                                                         defaultIndex);
}

// Filter and amp envelopes share their parameters; only defaults differ (01-parameters.md).
std::unique_ptr<Group> makeEnvelope (const char* groupId, const char* groupName, const char* prefix,
                                     const char* attackId, const char* decayId, const char* sustainId,
                                     const char* releaseId, float attack, float decay, float sustain, float release)
{
    const juce::String p (prefix);
    return std::make_unique<Group> (groupId, groupName, "|",
                                    makeFloat (attackId, (p + " Attack"), logRange (0.001f, 10.0f), attack, "s"),
                                    makeFloat (decayId, (p + " Decay"), logRange (0.001f, 15.0f), decay, "s"),
                                    makeFloat (sustainId, (p + " Sustain"), linearRange (0.0f, 1.0f), sustain, ""),
                                    makeFloat (releaseId, (p + " Release"), logRange (0.001f, 15.0f), release, "s"));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<Group> ("osc_a", "Oscillator A", "|",
                                         makeSemitones (id::oscAFreq, "Osc A Frequency", -24, 24, 0),
                                         makeBool (id::oscASaw, "Osc A Saw", true),
                                         makeBool (id::oscAPulse, "Osc A Pulse", false),
                                         makeFloat (id::oscAPw, "Osc A Pulse Width", linearRange (0.05f, 0.95f), 0.5f, ""),
                                         makeBool (id::oscASync, "Osc A Sync", false)));

    layout.add (std::make_unique<Group> ("osc_b", "Oscillator B", "|",
                                         makeSemitones (id::oscBFreq, "Osc B Frequency", -24, 24, 0),
                                         makeFloat (id::oscBFine, "Osc B Fine", linearRange (-50.0f, 50.0f), 0.0f, "cents"),
                                         makeBool (id::oscBSaw, "Osc B Saw", true),
                                         makeBool (id::oscBTri, "Osc B Triangle", false),
                                         makeBool (id::oscBPulse, "Osc B Pulse", false),
                                         makeFloat (id::oscBPw, "Osc B Pulse Width", linearRange (0.05f, 0.95f), 0.5f, ""),
                                         makeBool (id::oscBLoFreq, "Osc B Lo Freq", false),
                                         makeBool (id::oscBKbd, "Osc B Keyboard", true)));

    layout.add (std::make_unique<Group> ("mixer", "Mixer", "|",
                                         makeFloat (id::mixOscA, "Mixer Osc A", linearRange (0.0f, 1.0f), 1.0f, ""),
                                         makeFloat (id::mixOscB, "Mixer Osc B", linearRange (0.0f, 1.0f), 0.8f, ""),
                                         makeFloat (id::mixNoise, "Mixer Noise", linearRange (0.0f, 1.0f), 0.0f, "")));

    layout.add (std::make_unique<Group> ("filter", "Filter", "|",
                                         makeFloat (id::fltCutoff, "Filter Cutoff", logRange (20.0f, 20000.0f), 4000.0f, "Hz"),
                                         makeFloat (id::fltRes, "Filter Resonance", linearRange (0.0f, 1.0f), 0.0f, ""),
                                         makeFloat (id::fltEnvAmt, "Filter Env Amount", linearRange (0.0f, 1.0f), 0.4f, ""),
                                         makeChoice (id::fltKbd, "Filter Keyboard", { "Off", "Half", "Full" }, 0)));

    layout.add (makeEnvelope ("filter_env", "Filter Envelope", "Filter Env", id::fenvAttack, id::fenvDecay,
                              id::fenvSustain, id::fenvRelease, 0.005f, 0.6f, 0.4f, 0.3f));

    layout.add (makeEnvelope ("amp_env", "Amp Envelope", "Amp Env", id::aenvAttack, id::aenvDecay,
                              id::aenvSustain, id::aenvRelease, 0.002f, 0.5f, 1.0f, 0.2f));

    layout.add (std::make_unique<Group> ("poly_mod", "Poly-Mod", "|",
                                         makeFloat (id::pmFiltEnv, "Poly-Mod Filter Env", linearRange (0.0f, 1.0f), 0.0f, ""),
                                         makeFloat (id::pmOscB, "Poly-Mod Osc B", linearRange (0.0f, 1.0f), 0.0f, ""),
                                         makeBool (id::pmDestFreqA, juce::String::fromUTF8 ("Poly-Mod → Freq A"), false),
                                         makeBool (id::pmDestPwA, juce::String::fromUTF8 ("Poly-Mod → PW A"), false),
                                         makeBool (id::pmDestFilter, juce::String::fromUTF8 ("Poly-Mod → Filter"), false)));

    layout.add (std::make_unique<Group> ("lfo", "LFO", "|",
                                         makeFloat (id::lfoRate, "LFO Frequency", logRange (0.05f, 30.0f), 5.0f, "Hz"),
                                         makeChoice (id::lfoShape, "LFO Shape", { "Saw", "Triangle", "Square" }, 1)));

    layout.add (std::make_unique<Group> ("wheel_mod", "Wheel-Mod", "|",
                                         makeFloat (id::wmMix, "Wheel-Mod LFO/Noise", linearRange (0.0f, 1.0f), 0.0f, ""),
                                         makeBool (id::wmDestFreqA, juce::String::fromUTF8 ("Wheel-Mod → Freq A"), false),
                                         makeBool (id::wmDestFreqB, juce::String::fromUTF8 ("Wheel-Mod → Freq B"), false),
                                         makeBool (id::wmDestPwA, juce::String::fromUTF8 ("Wheel-Mod → PW A"), false),
                                         makeBool (id::wmDestPwB, juce::String::fromUTF8 ("Wheel-Mod → PW B"), false),
                                         makeBool (id::wmDestFilter, juce::String::fromUTF8 ("Wheel-Mod → Filter"), false)));

    FloatRange glideRange (0.0f, 2.0f);
    glideRange.setSkewForCentre (0.2f);

    layout.add (std::make_unique<Group> ("performance", "Performance", "|",
                                         makeFloat (id::perfGlide, "Glide", glideRange, 0.0f, "s"),
                                         makeFloat (id::perfVintage, "Vintage", linearRange (0.0f, 1.0f), 0.25f, ""),
                                         makeBool (id::perfUnison, "Unison", false),
                                         makeChoice (id::perfVoices, "Voices", { "5", "8", "10" }, 0),
                                         makeBool (id::perfVelocity, "Velocity", true),
                                         makeBool (id::perfAftertouch, "Aftertouch", true)));

    auto bendAttributes = juce::AudioParameterIntAttributes().withLabel ("st");

    layout.add (std::make_unique<Group> ("master", "Master", "|",
                                         makeFloat (id::masterTune, "Master Tune", linearRange (-100.0f, 100.0f), 0.0f, "cents"),
                                         makeFloat (id::masterVolume, "Master Volume", linearRange (-60.0f, 6.0f), 0.0f, "dB"),
                                         std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::bendRange, kVersionHint },
                                                                                    "Pitch Bend Range", 1, 12, 2, bendAttributes)));

    return layout;
}
} // namespace p5x::params
