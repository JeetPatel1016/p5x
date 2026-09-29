// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <array>

// Every host parameter ID (01-parameters.md). Permanent once milestone 1 ships: never rename or reuse.
namespace p5x::params::id
{
// Oscillator A
inline constexpr const char* oscAFreq = "osc_a_freq";
inline constexpr const char* oscASaw = "osc_a_saw";
inline constexpr const char* oscAPulse = "osc_a_pulse";
inline constexpr const char* oscAPw = "osc_a_pw";
inline constexpr const char* oscASync = "osc_a_sync";

// Oscillator B
inline constexpr const char* oscBFreq = "osc_b_freq";
inline constexpr const char* oscBFine = "osc_b_fine";
inline constexpr const char* oscBSaw = "osc_b_saw";
inline constexpr const char* oscBTri = "osc_b_tri";
inline constexpr const char* oscBPulse = "osc_b_pulse";
inline constexpr const char* oscBPw = "osc_b_pw";
inline constexpr const char* oscBLoFreq = "osc_b_lofreq";
inline constexpr const char* oscBKbd = "osc_b_kbd";

// Mixer
inline constexpr const char* mixOscA = "mix_osc_a";
inline constexpr const char* mixOscB = "mix_osc_b";
inline constexpr const char* mixNoise = "mix_noise";

// Filter
inline constexpr const char* fltCutoff = "flt_cutoff";
inline constexpr const char* fltRes = "flt_res";
inline constexpr const char* fltEnvAmt = "flt_env_amt";
inline constexpr const char* fltKbd = "flt_kbd";

// Filter envelope
inline constexpr const char* fenvAttack = "fenv_attack";
inline constexpr const char* fenvDecay = "fenv_decay";
inline constexpr const char* fenvSustain = "fenv_sustain";
inline constexpr const char* fenvRelease = "fenv_release";

// Amp envelope
inline constexpr const char* aenvAttack = "aenv_attack";
inline constexpr const char* aenvDecay = "aenv_decay";
inline constexpr const char* aenvSustain = "aenv_sustain";
inline constexpr const char* aenvRelease = "aenv_release";

// Poly-Mod
inline constexpr const char* pmFiltEnv = "pm_filt_env";
inline constexpr const char* pmOscB = "pm_osc_b";
inline constexpr const char* pmDestFreqA = "pm_dest_freq_a";
inline constexpr const char* pmDestPwA = "pm_dest_pw_a";
inline constexpr const char* pmDestFilter = "pm_dest_filter";

// LFO
inline constexpr const char* lfoRate = "lfo_rate";
inline constexpr const char* lfoShape = "lfo_shape";

// Wheel-Mod
inline constexpr const char* wmMix = "wm_mix";
inline constexpr const char* wmDestFreqA = "wm_dest_freq_a";
inline constexpr const char* wmDestFreqB = "wm_dest_freq_b";
inline constexpr const char* wmDestPwA = "wm_dest_pw_a";
inline constexpr const char* wmDestPwB = "wm_dest_pw_b";
inline constexpr const char* wmDestFilter = "wm_dest_filter";

// Performance (extras)
inline constexpr const char* perfGlide = "perf_glide";
inline constexpr const char* perfVintage = "perf_vintage";
inline constexpr const char* perfUnison = "perf_unison";
inline constexpr const char* perfVoices = "perf_voices";
inline constexpr const char* perfVelocity = "perf_velocity";
inline constexpr const char* perfAftertouch = "perf_aftertouch";

// Master
inline constexpr const char* masterTune = "master_tune";
inline constexpr const char* masterVolume = "master_volume";
inline constexpr const char* bendRange = "bend_range";
} // namespace p5x::params::id

namespace p5x::params
{
inline constexpr int kNumParameters = 50;

// Parameter index order (used by MIDI Learn tables); matches the processor's parameter order.
inline constexpr std::array<const char*, kNumParameters> kAllIds {
    id::oscAFreq, id::oscASaw, id::oscAPulse, id::oscAPw, id::oscASync,
    id::oscBFreq, id::oscBFine, id::oscBSaw, id::oscBTri, id::oscBPulse, id::oscBPw, id::oscBLoFreq, id::oscBKbd,
    id::mixOscA, id::mixOscB, id::mixNoise,
    id::fltCutoff, id::fltRes, id::fltEnvAmt, id::fltKbd,
    id::fenvAttack, id::fenvDecay, id::fenvSustain, id::fenvRelease,
    id::aenvAttack, id::aenvDecay, id::aenvSustain, id::aenvRelease,
    id::pmFiltEnv, id::pmOscB, id::pmDestFreqA, id::pmDestPwA, id::pmDestFilter,
    id::lfoRate, id::lfoShape,
    id::wmMix, id::wmDestFreqA, id::wmDestFreqB, id::wmDestPwA, id::wmDestPwB, id::wmDestFilter,
    id::perfGlide, id::perfVintage, id::perfUnison, id::perfVoices, id::perfVelocity, id::perfAftertouch,
    id::masterTune, id::masterVolume, id::bendRange,
};

// perf_voices options.
inline constexpr std::array<int, 3> kVoiceCounts { 5, 8, 10 };
} // namespace p5x::params
