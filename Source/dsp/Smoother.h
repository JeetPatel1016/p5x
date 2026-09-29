// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <algorithm>
#include <cmath>

namespace p5x::dsp
{
// Linear ramp to a target over a fixed time ("Lin N" in 01-parameters.md § Conventions).
class LinearSmoother
{
public:
    void prepare (double sampleRate, double timeMs) noexcept
    {
        rampSamples = std::max (1, static_cast<int> (std::lround (sampleRate * timeMs * 0.001)));
        reset (target);
    }

    void reset (float value) noexcept
    {
        current = target = value;
        remaining = 0;
    }

    void setTarget (float value) noexcept
    {
        if (value == target)
            return;

        target = value;
        remaining = rampSamples;
        step = (target - current) / static_cast<float> (rampSamples);
    }

    float next() noexcept
    {
        if (remaining > 0)
        {
            current += step;

            if (--remaining == 0)
                current = target;
        }

        return current;
    }

    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }
    bool isSmoothing() const noexcept { return remaining > 0; }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int rampSamples = 1, remaining = 0;
};
} // namespace p5x::dsp
