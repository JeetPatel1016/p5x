# Open questions

**Claude: don't resolve these yourself.** Use the value currently in the spec and ask the user when the question becomes blocking. When resolved, move the answer to DECISIONS.md and update the relevant doc. "Rec." is Claude's recommendation, not a decision.

| # | Question | Current placeholder | Blocking at |
|---|---|---|---|
| 1 | Verify against the Rev 3 technical manual: oscillator frequency knob span and stepping (currently ±24 semitones, semitone steps) | as in 01-parameters.md | milestone 4 tuning |
| 2 | Verify: does Osc B with Kbd off also ignore pitch bend and glide? And master tune? (02 drops master tune too; on the hardware master tune likely feeds every VCO, so rec.: master tune still applies) | ignores bend, glide and master tune | milestone 3 |
| 3 | Verify: first note after idle glides from the voice's last note, or no glide? | glides from last note | milestone 4 |
| 4 | Envelope time ranges (A 1 ms–10 s, D/R 1 ms–15 s): check against hardware measurements if available | P5X values | milestone 4 |
| 5 | LFO shapes: the hardware may allow combining shapes; we use a single choice. Keep radio? | radio | milestone 5 |
| 6 | Add a filter-cutoff destination for aftertouch? | Wheel-Mod amount only | after milestone 6 |
| 7 | Pitch bend range lives in the settings dialog (no panel control). OK? | settings dialog | milestone 5 |
| 8 | Modulation depths (Wheel-Mod ±12 st, Poly-Mod +48 st / +5 oct, filter env +8 oct): tune by ear | as in 05-modulation.md | milestone 4 |
| 9 | Factory preset list: names and categories for the 40 patches | none yet | milestone 6 |
| 10 | Milestone 7 SSM2040 mode needs its own spec (filter model + envelope curve differences) before starting | – | milestone 7 |
| 13 | Where does the preset **name** show? The chosen look has a 3-digit 7-segment readout, which can only show the number | small light-grey text line under the display | milestone 5 |
| 14 | The Main panel layout (control positions) only exists in the design canvas on claude.ai, which Claude Code can't open. Ask the user for a starting `art/layout.json` before milestone 5a | none in repo | milestone 5a |
| 15 | The Audio/MIDI settings and Debug console artboards aren't in the repo either. Screenshots or a description are needed to match them | plain functional layout, palette + Kode Mono | milestone 5 |
| 21 | Filter test conflict: the model's passband gain is 1/(1+k). At res 0.9, k = 3.63, which gives −13.3 dB, but the test expects a 3–8 dB drop. Change the test range (≈ 12–15 dB) or partly compensate in the model? | as in 03-filter.md | milestone 2 |
| 22 | Filter slope test: four ideal poles at fc = 1 kHz give 21.2 dB between 2 and 4 kHz (not 24 ± 2), because 2–4 × fc isn't yet asymptotic. Rec.: measure between 4 and 8 kHz (23.3 dB) | as in 03-filter.md | milestone 2 |
| 23 | Self-oscillation start-up: with k = 4.08 the small-signal growth rate is ≈ 0.031 × fc per second, so a 1e-6 impulse at fc = 100 Hz needs ≈ 4.4 s to reach full level. The "RMS stable between 1 s and 2 s" test fails at 100 Hz. Options: measure later at low fc, a larger excitation, or a larger k at max | as in 03-filter.md | milestone 2 |
| 24 | Saw aliasing test is ambiguous (a 1 kHz saw, or a sweep up to C8?), and −80 dB is likely out of reach for 2-sample PolyBLEP at 2×. Rec.: measure in milestone 2, then set the threshold with the user | as in 02-oscillators.md | milestone 2 |
| 25 | Filter sweep test "20 Hz → 20 kHz at 1 000 Hz rate": does that mean one sweep every 1 ms? | unspecified | milestone 2 |
| 26 | Filter keyboard tracking uses which note: the glided note, and does it include bend? Rec.: glided note, no bend | unspecified | milestone 2 |
| 27 | Safety clipper formula isn't continuous as written (tanh(0.8) = 0.66, not 0.8). Rec.: for \|x\| > 0.8, y = sign(x)·(0.8 + 0.2·tanh((\|x\| − 0.8)/0.2)), which is continuous in value and slope and peaks at ±1 | as in 06-voices.md | milestone 2 |
| 28 | Poly-Mod Osc B source is labelled ±1, but B's raw summed waveform reaches ±3 with all shapes on (02 doesn't normalise). Rec.: use the raw sum and fix the label | raw sum | milestone 3 |
| 29 | Glide for a voice's very first note (no previous note). Rec.: no glide | unspecified | milestone 4 |
| 30 | Unison with the sustain pedal: does the note stack keep pedal-held notes? | unspecified | milestone 4 |
| 31 | Accent colour for code-drawn overlays on the panel (learn ring, tooltips): 08 palette `#F0A93B` vs style.json `#EEC985`. Rec.: style.json on the panel; the 08 palette for the flat Debug/Settings windows | 08 palette | milestone 5 |
| 32 | "Every parameter has exactly one panel control" (08, 13) conflicts with `flt_kbd` (2 buttons) and `lfo_shape` (3 buttons). Rec.: one control per option, with `layout.json` ids like `lfo_shape#saw`, and a test that every option has exactly one | as in 08 / 13 | milestone 5 |
| 33 | `bend_range` is a host parameter, so 09 puts it in presets, yet it's set in the Settings dialog next to instance settings. Should presets carry it? | in presets | milestone 6 |
| 34 | Factory presets: is `Init` one of the 40 (indices 000–039, i.e. 39 patches + Init) or extra? | unspecified | milestone 6 |
| 35 | Preset file values for Bool and Choice params: "real units" isn't defined for them. Rec.: Bool as 0/1, Choice as the option label (`Half`) | unspecified | milestone 6 |
