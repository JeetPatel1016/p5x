// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

namespace p5x::dsp
{
// Milestone 1 placeholder voice: one sine per note with a fixed linear envelope (5 ms attack,
// 100 ms release), voice gain 0.2 (06-voices.md), velocity ignored (see DECISIONS.md, "Milestone 1 placeholder
// voice"). Milestone 2 replaces the internals with oscillators, filter and envelopes (06-voices.md).
class Voice
{
public:
    enum class Stage
    {
        Idle,
        Attack,
        Sustain,
        Release
    };

    static constexpr double kAttackSeconds = 0.005;
    static constexpr double kReleaseSeconds = 0.1;
    static constexpr float kVoiceGain = 0.2f; // 06-voices.md § Voice structure, step 9

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // Retriggers from the current level (no reset to zero, no click); see 04-envelopes.md § Gate behaviour.
    void start (int midiNote, float velocity01) noexcept;
    void release() noexcept;
    void kill() noexcept;

    bool isIdle() const noexcept { return stage == Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release; }
    Stage getStage() const noexcept { return stage; }
    int getNote() const noexcept { return note; }
    float getLevel() const noexcept { return level; }

    // Adds numSamples of output into `out`. pitchOffsetSemis[i] holds bend + master tune for sample i.
    void render (float* out, const float* pitchOffsetSemis, int numSamples) noexcept;

private:
    double sampleRate = 48000.0;
    double phase = 0.0;
    float level = 0.0f;
    float attackStep = 0.0f, releaseStep = 0.0f;
    int note = 60;
    Stage stage = Stage::Idle;
};
} // namespace p5x::dsp
