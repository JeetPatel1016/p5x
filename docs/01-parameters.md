# 01 · Parameters

This table is the complete parameter set. Don't add, remove or rename parameters without an approved change (see CLAUDE.md working rules).

**Source column:** `HW` = matches the original's behaviour as documented; `P5X` = our choice (tune by ear later, record changes in DECISIONS.md); `Verify` = believed to match the hardware but should be checked against the Rev 3 technical manual (tracked in OPEN_QUESTIONS.md). Until verified, use the value given.

## Conventions
- IDs are `snake_case`, prefixed by section. Declared once in `params/ParameterIDs.h` as `constexpr const char*`. Parameter version hint = 1 for all.
- **Continuous:** `AudioParameterFloat` with a `NormalisableRange`. "Log" = skew so the knob midpoint is the geometric mean of the range (`range.setSkewForCentre(sqrt(min*max))`).
- **Stepped:** `AudioParameterInt`. **Toggle:** `AudioParameterBool`. **Choice:** `AudioParameterChoice`.
- All parameters are automatable except those marked *State* (stored in plugin state, not exposed to the host).
- **Smoothing:** "Lin 20" = linear smoothing over 20 ms; "Log 20" = smoothing in the log domain (for frequencies); "–" = applied immediately at the next sample. Toggles and choices never smooth; they switch at the next sample boundary.

## Oscillator A
| ID | Name | Type | Range | Default | Unit / curve | Smooth | Source |
|---|---|---|---|---|---|---|---|
| `osc_a_freq` | Osc A Frequency | Int | −24 … +24 | 0 | semitones from played note | – | Verify (semitone-stepped, 4-octave span) |
| `osc_a_saw` | Osc A Saw | Bool | | on | | – | HW |
| `osc_a_pulse` | Osc A Pulse | Bool | | off | | – | HW |
| `osc_a_pw` | Osc A Pulse Width | Float | 0.05 … 0.95 | 0.50 | duty cycle, linear | Lin 20 | P5X (limits avoid silent pulse) |
| `osc_a_sync` | Osc A Sync | Bool | | off | hard sync A to B | – | HW |

## Oscillator B
| ID | Name | Type | Range | Default | Unit / curve | Smooth | Source |
|---|---|---|---|---|---|---|---|
| `osc_b_freq` | Osc B Frequency | Int | −24 … +24 | 0 | semitones | – | Verify |
| `osc_b_fine` | Osc B Fine | Float | −50 … +50 | 0 | cents, linear | Lin 20 | P5X |
| `osc_b_saw` | Osc B Saw | Bool | | on | | – | HW |
| `osc_b_tri` | Osc B Triangle | Bool | | off | | – | HW |
| `osc_b_pulse` | Osc B Pulse | Bool | | off | | – | HW |
| `osc_b_pw` | Osc B Pulse Width | Float | 0.05 … 0.95 | 0.50 | duty cycle | Lin 20 | P5X |
| `osc_b_lofreq` | Osc B Lo Freq | Bool | | off | see 02-oscillators.md | – | HW (range P5X) |
| `osc_b_kbd` | Osc B Keyboard | Bool | | on | off = B ignores note pitch | – | HW |

## Mixer
| ID | Name | Type | Range | Default | Unit / curve | Smooth | Source |
|---|---|---|---|---|---|---|---|
| `mix_osc_a` | Mixer Osc A | Float | 0 … 1 | 1.0 | linear gain | Lin 20 | HW |
| `mix_osc_b` | Mixer Osc B | Float | 0 … 1 | 0.8 | linear gain | Lin 20 | HW |
| `mix_noise` | Mixer Noise | Float | 0 … 1 | 0.0 | linear gain | Lin 20 | HW |

## Filter
| ID | Name | Type | Range | Default | Unit / curve | Smooth | Source |
|---|---|---|---|---|---|---|---|
| `flt_cutoff` | Filter Cutoff | Float | 20 … 20 000 | 4000 | Hz, log | Log 20 | P5X range |
| `flt_res` | Filter Resonance | Float | 0 … 1 | 0.0 | see 03-filter.md | Lin 20 | HW |
| `flt_env_amt` | Filter Env Amount | Float | 0 … 1 | 0.40 | 1.0 = +8 octaves, unipolar | Lin 20 | HW unipolar; depth P5X |
| `flt_kbd` | Filter Keyboard | Choice | Off, Half, Full | Off | tracking amount 0 / 0.5 / 1 | – | HW |

## Filter envelope / Amp envelope
Both envelopes have identical parameters; prefix `fenv_` for the filter envelope, `aenv_` for the amp envelope.

| ID suffix | Name | Type | Range | Default (fenv / aenv) | Unit / curve | Smooth | Source |
|---|---|---|---|---|---|---|---|
| `_attack` | Attack | Float | 0.001 … 10 | 0.005 / 0.002 | seconds, log | – (read every sample, see 04-envelopes.md) | P5X range |
| `_decay` | Decay | Float | 0.001 … 15 | 0.60 / 0.50 | seconds, log | – | P5X range |
| `_sustain` | Sustain | Float | 0 … 1 | 0.40 / 1.00 | level, linear | Lin 20 | HW |
| `_release` | Release | Float | 0.001 … 15 | 0.30 / 0.20 | seconds, log | – | P5X range |

## Poly-Mod
| ID | Name | Type | Range | Default | Unit / curve | Smooth | Source |
|---|---|---|---|---|---|---|---|
| `pm_filt_env` | Poly-Mod Filter Env | Float | 0 … 1 | 0.0 | depth, see 05-modulation.md | Lin 20 | HW |
| `pm_osc_b` | Poly-Mod Osc B | Float | 0 … 1 | 0.0 | depth | Lin 20 | HW |
| `pm_dest_freq_a` | Poly-Mod → Freq A | Bool | | off | | – | HW |
| `pm_dest_pw_a` | Poly-Mod → PW A | Bool | | off | | – | HW |
| `pm_dest_filter` | Poly-Mod → Filter | Bool | | off | | – | HW |

## LFO
| ID | Name | Type | Range | Default | Unit / curve | Smooth | Source |
|---|---|---|---|---|---|---|---|
| `lfo_rate` | LFO Frequency | Float | 0.05 … 30 | 5.0 | Hz, log | Log 20 | P5X range |
| `lfo_shape` | LFO Shape | Choice | Saw, Triangle, Square | Triangle | | – | P5X (radio; see OPEN_QUESTIONS) |

## Wheel-Mod
| ID | Name | Type | Range | Default | Unit / curve | Smooth | Source |
|---|---|---|---|---|---|---|---|
| `wm_mix` | Wheel-Mod LFO/Noise | Float | 0 … 1 | 0.0 | 0 = all LFO, 1 = all noise, linear crossfade | Lin 20 | HW |
| `wm_dest_freq_a` | Wheel-Mod → Freq A | Bool | | off | | – | HW |
| `wm_dest_freq_b` | Wheel-Mod → Freq B | Bool | | off | | – | HW |
| `wm_dest_pw_a` | Wheel-Mod → PW A | Bool | | off | | – | HW |
| `wm_dest_pw_b` | Wheel-Mod → PW B | Bool | | off | | – | HW |
| `wm_dest_filter` | Wheel-Mod → Filter | Bool | | off | | – | HW |

## Performance (extras)
| ID | Name | Type | Range | Default | Unit / curve | Smooth | Source |
|---|---|---|---|---|---|---|---|
| `perf_glide` | Glide | Float | 0 … 2 | 0.0 | seconds (constant time), log-ish skew centre 0.2 | – | P5X |
| `perf_vintage` | Vintage | Float | 0 … 1 | 0.25 | amount, see 06-voices.md | Lin 20 | P5X |
| `perf_unison` | Unison | Bool | | off | | – | P5X |
| `perf_voices` | Voices | Choice | 5, 8, 10 | 5 | | – (applied when all voices idle, or on next prepare) | P5X |
| `perf_velocity` | Velocity | Bool | | on | | – | P5X |
| `perf_aftertouch` | Aftertouch | Bool | | on | raises Wheel-Mod amount: max(wheel, pressure), see 05-modulation.md | – | P5X |

## Master
| ID | Name | Type | Range | Default | Unit / curve | Smooth | Source |
|---|---|---|---|---|---|---|---|
| `master_tune` | Master Tune | Float | −100 … +100 | 0 | cents | Lin 20 | P5X |
| `master_volume` | Master Volume | Float | −60 … +6 | 0 | dB (−60 = silence) | Lin 20 (in gain domain) | P5X |
| `bend_range` | Pitch Bend Range | Int | 1 … 12 | 2 | semitones up and down | – | P5X (no panel control; see OPEN_QUESTIONS) |

## State-only (not automatable, not in presets)
| Key | Type | Default | Where set |
|---|---|---|---|
| `midi_channel` | int 0–16 (0 = Omni) | 0 | Settings |
| `takeover_mode` | Pickup / Jump | Pickup | Settings |
| `hq_mode` | bool (4× oversampling) | false | Settings |
| `program_change_enabled` | bool | false | Settings |
| `seed` | uint64 | random at first construction, then persisted | internal |
| `scale` | float, window scale 0.75–1.5 | 1.0 | editor resize |
| `midi_map` | CC → param ID list | empty | MIDI Learn |

## Total
50 host parameters (42 listed individually + 8 envelope parameters). The count is asserted in a unit test so accidental additions fail CI.
