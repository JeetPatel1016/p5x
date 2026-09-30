// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "dsp/Envelope.h"
#include "dsp/LadderFilterCEM.h"
#include "dsp/Oscillator.h"
#include "dsp/Random.h"

#include <cstdint>

namespace p5x::dsp
{
// What every voice reads while rendering one chunk. Arrays hold one value per internal
// (oversampled) sample; they come from the processor's parameter smoothers.
struct VoiceInputs
{
    const float* pitchOffset = nullptr; // bend + master tune, semitones (Osc A; Osc B when Kbd is on)
    const float* pwA = nullptr;
    const float* pwB = nullptr;
    const float* fineB = nullptr; // cents
    const float* mixA = nullptr;
    const float* mixB = nullptr;
    const float* mixNoise = nullptr;
    const float* cutoffHz = nullptr;
    const float* resonance = nullptr;
    const float* envAmount = nullptr;
    const float* filterSustain = nullptr;
    const float* ampSustain = nullptr;
    const float* externalInput = nullptr; // Standalone "Route input into filter": unity into the mixer, or null

    int oscAFreq = 0, oscBFreq = 0; // semitones
    OscShapes shapesA, shapesB;
    bool oscBKeyboard = true;
    float keyboardTrack = 0.0f; // 0, 0.5, 1
    EnvParams filterEnv, ampEnv;  // times; sustain comes from the arrays above
};

// One voice (06-voices.md § Voice structure). Milestone 2: oscillators, noise, mixer, CEM3320
// filter, both envelopes, VCA, DC blocker. Poly-Mod, Wheel-Mod and sync (milestone 3) and velocity,
// glide, Vintage and unison (milestone 4) come later.
class Voice
{
public:
    static constexpr float kVoiceGain = 0.16f;  // 06-voices.md § Voice structure, step 9
    static constexpr float kNoiseLevel = 0.7f;  // 02-oscillators.md § Noise
    static constexpr float kFilterDrive = 0.5f; // 03-filter.md § Core algorithm
    static constexpr double kDcBlockerHz = 5.0; // 06-voices.md, step 8

    // Seeds the voice's Random from the instance seed and voice index, so repeated prepare calls
    // and reloads give the same initial phases and noise (00-architecture.md § Randomness).
    void prepare (double internalRate, uint64_t seed, int voiceIndex) noexcept;
    void reset() noexcept;

    // Retriggers both envelopes from their current level (04-envelopes.md § Gate behaviour).
    void start (int midiNote, float velocity01) noexcept;
    void release() noexcept;
    void kill() noexcept; // hard cut to idle (All Sound Off, Panic)

    // A voice is free when its amp envelope is idle (04-envelopes.md § Voice lifetime).
    bool isIdle() const noexcept { return ampEnvelope.isIdle(); }
    bool isReleasing() const noexcept { return ampEnvelope.stage() == Envelope::Stage::Release; }

    // Adds numSamples of output into `out` (internal rate). Idle voices do nothing.
    void render (float* out, const VoiceInputs& in, int numSamples) noexcept;

    // Telemetry (10-debug-and-harness.md § Telemetry)
    int getNote() const noexcept { return note; }
    float getVelocity() const noexcept { return velocity; }
    float getOscAHz() const noexcept { return lastOscAHz; }
    float getCutoffHz() const noexcept { return lastCutoffHz; }
    Envelope::Stage getAmpStage() const noexcept { return ampEnvelope.stage(); }
    float getAmpLevel() const noexcept { return ampEnvelope.getLevel(); }
    float getFilterEnvLevel() const noexcept { return filterEnvelope.getLevel(); }
    double getOscAPhase() const noexcept { return oscA.getPhase(); }
    double getOscBPhase() const noexcept { return oscB.getPhase(); }

private:
    double rate = 96000.0;
    Random random;
    Oscillator oscA, oscB;
    LadderFilterCEM filter;
    Envelope filterEnvelope, ampEnvelope;

    double dcCoeff = 0.0, dcIn = 0.0, dcOut = 0.0;
    int note = 60;
    float velocity = 1.0f;
    float lastOscAHz = 0.0f, lastCutoffHz = 0.0f;
};
} // namespace p5x::dsp
