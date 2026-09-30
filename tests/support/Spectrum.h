// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

// Small measurement helpers for DSP tests: radix-2 FFT magnitude spectrum, RMS, level in dB.

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

namespace p5x::test
{
inline void fftInPlace (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();

    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;

        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;

        j ^= bit;

        if (i < j)
            std::swap (a[i], a[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double angle = -2.0 * std::numbers::pi / (double) len;
        const std::complex<double> wLen (std::cos (angle), std::sin (angle));

        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);

            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k];
                const auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wLen;
            }
        }
    }
}

// Magnitude spectrum (bins 0 … N/2) of `signal` with a Blackman-Harris window; N = signal size (power of 2).
inline std::vector<double> magnitudeSpectrum (const std::vector<float>& signal)
{
    const size_t n = signal.size();
    std::vector<std::complex<double>> a (n);

    for (size_t i = 0; i < n; ++i)
    {
        const double x = 2.0 * std::numbers::pi * (double) i / (double) (n - 1);
        const double w = 0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2 * x) - 0.01168 * std::cos (3 * x);
        a[i] = { signal[i] * w, 0.0 };
    }

    fftInPlace (a);
    std::vector<double> mag (n / 2 + 1);

    for (size_t k = 0; k <= n / 2; ++k)
        mag[k] = std::abs (a[k]);

    return mag;
}

// Peak magnitude within ±width bins of `bin` (the window spreads a sinusoid over a few bins).
inline double peakAround (const std::vector<double>& mag, double bin, int width = 4)
{
    double best = 0.0;
    const int centre = (int) std::lround (bin);

    for (int k = std::max (0, centre - width); k <= std::min ((int) mag.size() - 1, centre + width); ++k)
        best = std::max (best, mag[(size_t) k]);

    return best;
}

inline double toDb (double ratio)
{
    return 20.0 * std::log10 (std::max (ratio, 1.0e-30));
}

inline double rms (const std::vector<float>& x, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > x.size())
        to = x.size();

    double sum = 0.0;

    for (size_t i = from; i < to; ++i)
        sum += (double) x[i] * x[i];

    return to > from ? std::sqrt (sum / (double) (to - from)) : 0.0;
}
} // namespace p5x::test
