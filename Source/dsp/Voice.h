// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "dsp/Envelope.h"
#include "dsp/LadderFilterCEM.h"
#include "dsp/Oscillator.h"
#include "dsp/Random.h"
#include "dsp/Smoother.h"

#include <cstdint>

namespace p5x::dsp
{
// What every voice reads while rendering one chunk. Arrays hold one value per internal
// (oversampled) sample; they come from the processor's parameter smoothers.
struct VoiceInputs
{
    const float* pitchOffset = nullptr; // bend + master tune, semitones (Osc A; Osc B when Kbd is on)
    const float* masterTune = nullptr;  // master tune alone, semitones (Osc B when Kbd is off)
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

    // Modulation (05-modulation.md). Wheel-Mod = source (LFO/noise, ±1) × amount (wheel, 0..1).
    const float* wheelModSource = nullptr;
    const float* wheelModAmount = nullptr;
    const float* channelPressure = nullptr; // smoothed channel aftertouch, 0..1
    const float* pmFilterEnv = nullptr;     // Poly-Mod amounts, 0..1
    const float* pmOscB = nullptr;
    const float* vintage = nullptr; // perf_vintage, 0..1 (06-voices.md § Vintage)

    int oscAFreq = 0, oscBFreq = 0; // semitones
    OscShapes shapesA, shapesB;
    bool syncA = false, oscBLoFreq = false;
    bool oscBKeyboard = true;
    bool wmFreqA = false, wmFreqB = false, wmPwA = false, wmPwB = false, wmFilter = false;
    bool pmFreqA = false, pmPwA = false, pmFilter = false;
    bool velocityOn = false;   // perf_velocity
    bool aftertouchOn = false; // perf_aftertouch
    float keyboardTrack = 0.0f; // 0, 0.5, 1
    EnvParams filterEnv, ampEnv;  // times; sustain comes from the arrays above
};

// One voice (06-voices.md § Voice structure): oscillators with sync and Lo Freq, noise, mixer,
// Poly-Mod, Wheel-Mod destinations with aftertouch, CEM3320 filter, both envelopes, VCA with
// velocity, glide, Vintage offsets and drift, unison detune and gain.
class Voice
{
public:
    static constexpr float kVoiceGain = 0.16f;  // 06-voices.md § Voice structure, step 9
    static constexpr float kNoiseLevel = 0.7f;  // 02-oscillators.md § Noise
    static constexpr float kFilterDrive = 0.5f; // 03-filter.md § Core algorithm
    static constexpr double kLoFreqDivisor = 128.0; // 02-oscillators.md § Pitch

    // Modulation depths at full source (05-modulation.md).
    static constexpr double kWheelModSemitones = 12.0, kWheelModOctaves = 4.0;
    static constexpr double kPolyModSemitones = 48.0, kPolyModOctaves = 5.0;
    static constexpr float kModPulseWidth = 0.45f;

    // Vintage at 1 (06-voices.md § Vintage).
    static constexpr double kVintageDetuneCents = 8.0, kVintageCutoffOctaves = 0.2, kVintageEnvScale = 0.1;
    static constexpr double kDriftCents = 4.0, kDriftCrossSeconds = 10.0, kDriftLowPassHz = 0.3;

    // Seeds the voice's Random from the instance seed and voice index, so repeated prepare calls
    // and reloads give the same initial phases, noise and Vintage offsets (00-architecture.md
    // § Randomness). Vintage draws from its own stream so Vintage 0 leaves phases and noise as they
    // were. Drift updates once per host sample (every `oversampling` internal samples).
    void prepare (double internalRate, uint64_t seed, int voiceIndex, int oversampling = 2) noexcept;
    void reset() noexcept;

    // Retriggers both envelopes from their current level (04-envelopes.md § Gate behaviour) and
    // glides to the note over glideSeconds: from `fromNote` when given (unison), else from where the
    // voice's pitch is now. A voice's first note since prepare never glides (06-voices.md § Glide).
    void start (int midiNote, float velocity01, double glideSeconds = 0.0, double fromNote = -1.0) noexcept;

    // Moves to another note without retriggering (unison falling back to a held note).
    void glideTo (int midiNote, double glideSeconds) noexcept;

    void release() noexcept;
    void kill() noexcept; // hard cut to idle (All Sound Off, Panic)

    void setPressure (float pressure01) noexcept { pressure.setTarget (pressure01); } // poly aftertouch
    void setUnison (double detuneCents, float gainScale) noexcept
    {
        unisonCents = detuneCents;
        unisonGain = gainScale;
    }

    // A voice is free when its amp envelope is idle (04-envelopes.md § Voice lifetime).
    bool isIdle() const noexcept { return ampEnvelope.isIdle(); }
    bool isReleasing() const noexcept { return ampEnvelope.stage() == Envelope::Stage::Release; }

    // Adds numSamples of output into `out` (internal rate). Idle voices do nothing.
    void render (float* out, const VoiceInputs& in, int numSamples) noexcept;

    // Telemetry (10-debug-and-harness.md § Telemetry)
    int getNote() const noexcept { return note; }
    double getPitch() const noexcept { return pitch; } // the glided note, semitones
    float getVelocity() const noexcept { return velocity; }
    float getOscAHz() const noexcept { return lastOscAHz; }
    float getOscBHz() const noexcept { return lastOscBHz; }
    float getCutoffHz() const noexcept { return lastCutoffHz; }
    Envelope::Stage getAmpStage() const noexcept { return ampEnvelope.stage(); }
    float getAmpLevel() const noexcept { return ampEnvelope.getLevel(); }
    float getFilterEnvLevel() const noexcept { return filterEnvelope.getLevel(); }
    double getOscAPhase() const noexcept { return oscA.getPhase(); }
    double getOscBPhase() const noexcept { return oscB.getPhase(); }

    // Vintage offsets at Vintage = 1 (for tests and the dump).
    double getVintageDetuneA() const noexcept { return vintageDetuneA; }
    double getVintageDetuneB() const noexcept { return vintageDetuneB; }
    double getVintageCutoff() const noexcept { return vintageCutoff; }
    double getVintageEnvScale() const noexcept { return vintageEnv; }
    double getDriftA() const noexcept { return driftA.value; }

private:
    // Bounded random walk (reflects at ±kDriftCents), then a 0.3 Hz one-pole (06 § Vintage).
    struct Drift
    {
        double walk = 0.0, value = 0.0;
        void step (Random& r, double size, double lowPass) noexcept;
    };

    void beginGlide (double from, int target, double glideSeconds) noexcept;

    double rate = 96000.0;
    Random random, vintageRandom;
    Oscillator oscA, oscB;
    LadderFilterCEM filter;
    Envelope filterEnvelope, ampEnvelope;
    LinearSmoother pressure;

    int note = 60;
    double pitch = 60.0, glideStep = 0.0;
    int glideRemaining = 0;
    bool hasPlayed = false;
    float velocity = 1.0f;

    double unisonCents = 0.0;
    float unisonGain = 1.0f;

    double vintageDetuneA = 0.0, vintageDetuneB = 0.0, vintageCutoff = 0.0, vintageEnv = 0.0; // at Vintage 1
    Drift driftA, driftB;
    double driftStepSize = 0.0, driftLowPass = 0.0;
    int driftEvery = 2, driftCountdown = 0;

    float lastOscAHz = 0.0f, lastOscBHz = 0.0f, lastCutoffHz = 0.0f;
};
} // namespace p5x::dsp
