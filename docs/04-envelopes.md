# 04 · Envelopes (CEM3310 model)

Two ADSR envelopes per voice: filter envelope (`fenv_*`) and amp envelope (`aenv_*`). Same class, different parameters.

## Interface (sketch)
```cpp
namespace p5x::dsp {
class Envelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    void prepare (double internalRate);
    void reset();                              // to Idle, level 0
    void gateOn();                             // (re)trigger from current level
    void gateOff();                            // go to Release from current level
    float process (const EnvParams&);          // returns level 0..1
    Stage stage() const;
    bool isIdle() const;
};
struct EnvParams { float attackS, decayS, sustain, releaseS; };
}
```

## Curves (analog RC behaviour)
All segments are one-pole RC curves toward a target, updated per sample:
`level += (target - level) * coeff`, with `coeff = 1 - exp(-1 / (tau * rate))`.

| Stage | Target | Ends when | tau from time param |
|---|---|---|---|
| Attack | **1.3** (overshoot target, gives the CEM3310's convex attack) | level ≥ 1.0 → clamp to 1.0, go to Decay | chosen so reaching 1.0 from 0 takes `attackS`: `tau = attackS / ln(1.3 / 0.3)` |
| Decay | `sustain` | never ends on its own; becomes Sustain when `|level - sustain| < 1e-4` | `tau = decayS / ln(1000)` (time param = time to get within 60 dB of target) |
| Sustain | follows `sustain` param (smoothed) | gateOff | – |
| Release | 0 | level < 1e-5 → Idle, level 0 | `tau = releaseS / ln(1000)` |

- Time parameters are read **every sample** (so turning Decay mid-note changes the curve immediately, like hardware pots).
- Attack from a non-zero level (retrigger) uses the same `tau`, so it reaches 1.0 sooner. That's correct RC behaviour.

## Gate behaviour
- **Retrigger on every note-on**, including when a voice is stolen or re-used: `gateOn()` restarts Attack **from the current level** (no reset to zero, no click). This is the hardware behaviour.
- `gateOff()` in any stage jumps to Release from the current level.
- Legato (unison mode, overlapping notes) still retriggers. (P5X, matches hardware multi-trigger.)
- Sustain pedal holds the gate (07-midi.md); the envelope itself knows nothing about the pedal.

## Voice lifetime
A voice is free when the **amp** envelope is Idle. The filter envelope's state doesn't matter.

## Velocity
Velocity scaling is applied by the voice, not inside the envelope (05-modulation.md).

## Tests
- Attack 10 ms at 96 kHz: reaches 1.0 at 10 ms ± 0.2 ms; level at 5 ms is 0.675 ± 0.02 (above the straight line, as RC charging toward 1.3 should be).
- Decay 1 s to sustain 0: level ≤ 0.001 at 1.0 s ± 2 %.
- Release 0.5 s from 1.0: level ≤ 0.001 at 0.5 s ± 2 %; Idle (level < 1e-5) at 0.83 s ± 2 %; level monotonic decreasing.
- Retrigger during Release at level 0.3: next sample ≥ 0.3 (no drop to 0).
- gateOff during Attack at level 0.5: next sample ≤ 0.5 and decreasing.
- Changing sustain while in Sustain moves the level smoothly (no step > 0.01 per sample), with the Lin 20 sustain smoother from 01-parameters.md in front of the envelope.
- Extreme params (0.001 s everywhere, 15 s everywhere) → no NaN, correct stage order.
