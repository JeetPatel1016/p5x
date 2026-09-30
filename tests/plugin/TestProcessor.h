// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "PluginProcessor.h"

#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>

namespace p5x::test
{
inline juce::RangedAudioParameter& param (P5XAudioProcessor& p, const char* id)
{
    auto* parameter = p.getParameterByIndex (p.getParameterIndex (id));
    jassert (parameter != nullptr);
    return *parameter;
}

// Sets a parameter in real units, as the host would.
inline void setParam (P5XAudioProcessor& p, const char* id, float realValue)
{
    auto& parameter = param (p, id);
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (realValue));
}

inline float getParam (P5XAudioProcessor& p, const char* id)
{
    auto& parameter = param (p, id);
    return parameter.convertFrom0to1 (parameter.getValue());
}

// A near-sine patch for pitch measurements: Osc A saw only, low fixed cutoff, no filter envelope,
// so each period has one rising zero crossing.
inline void setPurePatch (P5XAudioProcessor& p)
{
    setParam (p, "mix_osc_b", 0.0f);
    setParam (p, "flt_cutoff", 700.0f);
    setParam (p, "flt_env_amt", 0.0f);
}

struct TimedMessage
{
    juce::MidiMessage message;
    int samplePosition = 0;
};

inline juce::AudioBuffer<float> process (P5XAudioProcessor& p, int numSamples, std::initializer_list<TimedMessage> events = {})
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    juce::MidiBuffer midi;

    for (const auto& e : events)
        midi.addEvent (e.message, e.samplePosition);

    p.processBlock (buffer, midi);
    return buffer;
}

inline float peak (const juce::AudioBuffer<float>& buffer, int start = 0)
{
    float result = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = start; i < buffer.getNumSamples(); ++i)
            result = std::max (result, std::abs (buffer.getSample (ch, i)));

    return result;
}

inline double frequency (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const float* data = buffer.getReadPointer (0);
    int crossings = 0, first = -1, last = -1;

    for (int i = 1; i < buffer.getNumSamples(); ++i)
    {
        if (data[i - 1] < 0.0f && data[i] >= 0.0f)
        {
            if (first < 0)
                first = i;

            last = i;
            ++crossings;
        }
    }

    return crossings > 1 ? (crossings - 1) * sampleRate / (last - first) : 0.0;
}

// Log entries for this processor's instance written since `sequence` (which is advanced).
inline std::vector<std::string> logLines (P5XAudioProcessor& p, uint64_t& sequence)
{
    p.getLogService().drainNow();
    std::vector<debug::LogEntry> entries;
    sequence = p.getLogService().copyEntriesSince (sequence, entries);

    std::vector<std::string> lines;

    for (const auto& e : entries)
        if (e.instanceId == p.getInstanceId())
            lines.push_back (std::string (debug::levelName ((debug::Level) e.level)) + " " + e.text);

    return lines;
}

inline bool containsLine (const std::vector<std::string>& lines, const std::string& levelAndText)
{
    for (const auto& line : lines)
        if (line.find (levelAndText) != std::string::npos)
            return true;

    return false;
}
} // namespace p5x::test
