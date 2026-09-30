// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

namespace p5x::dsp
{
// The global LFO (05-modulation.md § LFO): free-running, bipolar ±1, runs at the host rate.
class Lfo
{
public:
    enum class Shape
    {
        Saw,
        Triangle,
        Square
    };

    static constexpr double kMinRateHz = 0.05, kMaxRateHz = 30.0;
    static constexpr double kSquareSlewSeconds = 0.001; // a full −1 → +1 swing takes 1 ms

    void prepare (double hostRate, double initialPhase) noexcept;

    // One host-rate sample; never resets on note-on.
    float process (double rateHz, Shape shape) noexcept;

    double getPhase() const noexcept { return phase; }

private:
    double rate = 48000.0;
    double phase = 0.0;
    float square = 1.0f; // slewed square, tracked for every shape so switching to it doesn't jump twice
    float squareStep = 0.0f;
};
} // namespace p5x::dsp
