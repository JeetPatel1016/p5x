# Open questions

**Claude: don't resolve these yourself.** Use the value currently in the spec and ask the user when the question becomes blocking. When resolved, move the answer to DECISIONS.md and update the relevant doc. "Rec." is Claude's recommendation, not a decision.

| # | Question | Current placeholder | Blocking at |
|---|---|---|---|
| 1 | Verify against the Rev 3 technical manual: oscillator frequency knob span and stepping (currently ±24 semitones, semitone steps) | as in 01-parameters.md | milestone 4 tuning |
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
| 29 | Glide for a voice's very first note (no previous note). Rec.: no glide | unspecified | milestone 4 |
| 30 | Unison with the sustain pedal: does the note stack keep pedal-held notes? | unspecified | milestone 4 |
| 31 | Accent colour for code-drawn overlays on the panel (learn ring, tooltips): 08 palette `#F0A93B` vs style.json `#EEC985`. Rec.: style.json on the panel; the 08 palette for the flat Debug/Settings windows | 08 palette | milestone 5 |
| 32 | "Every parameter has exactly one panel control" (08, 13) conflicts with `flt_kbd` (2 buttons) and `lfo_shape` (3 buttons). Rec.: one control per option, with `layout.json` ids like `lfo_shape#saw`, and a test that every option has exactly one | as in 08 / 13 | milestone 5 |
| 33 | `bend_range` is a host parameter, so 09 puts it in presets, yet it's set in the Settings dialog next to instance settings. Should presets carry it? | in presets | milestone 6 |
| 34 | Factory presets: is `Init` one of the 40 (indices 000–039, i.e. 39 patches + Init) or extra? | unspecified | milestone 6 |
| 35 | Preset file values for Bool and Choice params: "real units" isn't defined for them. Rec.: Bool as 0/1, Choice as the option label (`Half`) | unspecified | milestone 6 |
