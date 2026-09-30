// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
//
// Checks the parameter set against 01-parameters.md, written out independently of ParameterLayout.cpp.

#include "PluginProcessor.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <set>
#include <vector>

using Catch::Approx;

namespace
{
enum class Type
{
    Float,
    Int,
    Bool,
    Choice
};

enum class Curve
{
    Linear,
    Log,   // knob midpoint = geometric mean of the range
    Centre // explicit skew centre
};

struct Spec
{
    const char* id;
    const char* name;
    Type type;
    float min, max, defaultValue;
    Curve curve = Curve::Linear;
    float centre = 0.0f;
    std::vector<const char*> choices {};
};

std::vector<Spec> specTable()
{
    const auto B = Type::Bool;
    const auto F = Type::Float;
    const auto I = Type::Int;
    const auto C = Type::Choice;

    return {
        // Oscillator A
        { "osc_a_freq", "Osc A Frequency", I, -24, 24, 0 },
        { "osc_a_saw", "Osc A Saw", B, 0, 1, 1 },
        { "osc_a_pulse", "Osc A Pulse", B, 0, 1, 0 },
        { "osc_a_pw", "Osc A Pulse Width", F, 0.05f, 0.95f, 0.5f },
        { "osc_a_sync", "Osc A Sync", B, 0, 1, 0 },
        // Oscillator B
        { "osc_b_freq", "Osc B Frequency", I, -24, 24, 0 },
        { "osc_b_fine", "Osc B Fine", F, -50, 50, 0 },
        { "osc_b_saw", "Osc B Saw", B, 0, 1, 1 },
        { "osc_b_tri", "Osc B Triangle", B, 0, 1, 0 },
        { "osc_b_pulse", "Osc B Pulse", B, 0, 1, 0 },
        { "osc_b_pw", "Osc B Pulse Width", F, 0.05f, 0.95f, 0.5f },
        { "osc_b_lofreq", "Osc B Lo Freq", B, 0, 1, 0 },
        { "osc_b_kbd", "Osc B Keyboard", B, 0, 1, 1 },
        // Mixer
        { "mix_osc_a", "Mixer Osc A", F, 0, 1, 1.0f },
        { "mix_osc_b", "Mixer Osc B", F, 0, 1, 0.8f },
        { "mix_noise", "Mixer Noise", F, 0, 1, 0.0f },
        // Filter
        { "flt_cutoff", "Filter Cutoff", F, 20, 20000, 4000, Curve::Log },
        { "flt_res", "Filter Resonance", F, 0, 1, 0 },
        { "flt_env_amt", "Filter Env Amount", F, 0, 1, 0.4f },
        { "flt_kbd", "Filter Keyboard", C, 0, 2, 0, Curve::Linear, 0, { "Off", "Half", "Full" } },
        // Filter envelope
        { "fenv_attack", "Filter Env Attack", F, 0.001f, 10, 0.005f, Curve::Log },
        { "fenv_decay", "Filter Env Decay", F, 0.001f, 15, 0.6f, Curve::Log },
        { "fenv_sustain", "Filter Env Sustain", F, 0, 1, 0.4f },
        { "fenv_release", "Filter Env Release", F, 0.001f, 15, 0.3f, Curve::Log },
        // Amp envelope
        { "aenv_attack", "Amp Env Attack", F, 0.001f, 10, 0.002f, Curve::Log },
        { "aenv_decay", "Amp Env Decay", F, 0.001f, 15, 0.5f, Curve::Log },
        { "aenv_sustain", "Amp Env Sustain", F, 0, 1, 1.0f },
        { "aenv_release", "Amp Env Release", F, 0.001f, 15, 0.2f, Curve::Log },
        // Poly-Mod
        { "pm_filt_env", "Poly-Mod Filter Env", F, 0, 1, 0 },
        { "pm_osc_b", "Poly-Mod Osc B", F, 0, 1, 0 },
        { "pm_dest_freq_a", "Poly-Mod \xe2\x86\x92 Freq A", B, 0, 1, 0 },
        { "pm_dest_pw_a", "Poly-Mod \xe2\x86\x92 PW A", B, 0, 1, 0 },
        { "pm_dest_filter", "Poly-Mod \xe2\x86\x92 Filter", B, 0, 1, 0 },
        // LFO
        { "lfo_rate", "LFO Frequency", F, 0.05f, 30, 5, Curve::Log },
        { "lfo_shape", "LFO Shape", C, 0, 2, 1, Curve::Linear, 0, { "Saw", "Triangle", "Square" } },
        // Wheel-Mod
        { "wm_mix", "Wheel-Mod LFO/Noise", F, 0, 1, 0 },
        { "wm_dest_freq_a", "Wheel-Mod \xe2\x86\x92 Freq A", B, 0, 1, 0 },
        { "wm_dest_freq_b", "Wheel-Mod \xe2\x86\x92 Freq B", B, 0, 1, 0 },
        { "wm_dest_pw_a", "Wheel-Mod \xe2\x86\x92 PW A", B, 0, 1, 0 },
        { "wm_dest_pw_b", "Wheel-Mod \xe2\x86\x92 PW B", B, 0, 1, 0 },
        { "wm_dest_filter", "Wheel-Mod \xe2\x86\x92 Filter", B, 0, 1, 0 },
        // Performance
        { "perf_glide", "Glide", F, 0, 2, 0, Curve::Centre, 0.2f },
        { "perf_vintage", "Vintage", F, 0, 1, 0.25f },
        { "perf_unison", "Unison", B, 0, 1, 0 },
        { "perf_voices", "Voices", C, 0, 2, 0, Curve::Linear, 0, { "5", "8", "10" } },
        { "perf_velocity", "Velocity", B, 0, 1, 1 },
        { "perf_aftertouch", "Aftertouch", B, 0, 1, 1 },
        // Master
        { "master_tune", "Master Tune", F, -100, 100, 0 },
        { "master_volume", "Master Volume", F, -60, 6, 0 },
        { "bend_range", "Pitch Bend Range", I, 1, 12, 2 },
    };
}
} // namespace

TEST_CASE ("Parameters: exactly 50 host parameters", "[plugin][params]")
{
    P5XAudioProcessor processor;
    REQUIRE (processor.getParameters().size() == 50);
    REQUIRE (p5x::params::kNumParameters == 50);
    REQUIRE (specTable().size() == 50);
}

TEST_CASE ("Parameters: IDs are unique and in kAllIds order", "[plugin][params]")
{
    P5XAudioProcessor processor;
    std::set<juce::String> ids;

    for (int i = 0; i < processor.getParameters().size(); ++i)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getParameters()[i]);
        REQUIRE (p != nullptr);
        REQUIRE (p->getParameterID() == p5x::params::kAllIds[(size_t) i]);
        ids.insert (p->getParameterID());
    }

    REQUIRE (ids.size() == 50);
}

TEST_CASE ("Parameters: every parameter matches the spec table", "[plugin][params]")
{
    P5XAudioProcessor processor;

    for (const auto& spec : specTable())
    {
        INFO (spec.id);
        const int index = processor.getParameterIndex (spec.id);
        REQUIRE (index >= 0);

        auto* p = processor.getParameterByIndex (index);
        REQUIRE (p->getName (100) == juce::String::fromUTF8 (spec.name));
        REQUIRE (p->getVersionHint() == 1);
        REQUIRE (p->isAutomatable());

        const auto& range = p->getNormalisableRange();
        REQUIRE (range.start == Approx (spec.min));
        REQUIRE (range.end == Approx (spec.max));
        REQUIRE (p->convertFrom0to1 (p->getDefaultValue()) == Approx (spec.defaultValue).margin (1.0e-4));

        switch (spec.type)
        {
            case Type::Float:
                REQUIRE (dynamic_cast<juce::AudioParameterFloat*> (p) != nullptr);
                break;

            case Type::Int:
                REQUIRE (dynamic_cast<juce::AudioParameterInt*> (p) != nullptr);
                break;

            case Type::Bool:
                REQUIRE (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr);
                break;

            case Type::Choice:
            {
                auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p);
                REQUIRE (choice != nullptr);
                REQUIRE (choice->choices.size() == (int) spec.choices.size());

                for (size_t i = 0; i < spec.choices.size(); ++i)
                    REQUIRE (choice->choices[(int) i] == spec.choices[i]);

                break;
            }
        }

        if (spec.curve == Curve::Log)
            REQUIRE (p->convertFrom0to1 (0.5f) == Approx (std::sqrt (spec.min * spec.max)).epsilon (0.005));
        else if (spec.curve == Curve::Centre)
            REQUIRE (p->convertFrom0to1 (0.5f) == Approx (spec.centre).epsilon (0.005));
        else if (spec.type == Type::Float)
            REQUIRE (p->convertFrom0to1 (0.5f) == Approx ((spec.min + spec.max) * 0.5f).margin (1.0e-4));
    }
}

TEST_CASE ("Parameters: value text for tooltips", "[plugin][params]")
{
    P5XAudioProcessor processor;
    auto* cutoff = processor.getParameterByIndex (processor.getParameterIndex ("flt_cutoff"));
    REQUIRE (cutoff->getCurrentValueAsText() == "4000");
    REQUIRE (cutoff->getLabel() == "Hz");

    auto* freq = processor.getParameterByIndex (processor.getParameterIndex ("osc_a_freq"));
    freq->setValueNotifyingHost (freq->convertTo0to1 (7.0f));
    REQUIRE (freq->getCurrentValueAsText() == "+7");
    REQUIRE (freq->getLabel() == "st");
}
