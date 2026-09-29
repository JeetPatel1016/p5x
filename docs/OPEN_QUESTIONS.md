# Open questions

**Claude: don't resolve these yourself.** Use the value currently in the spec and ask the user when the question becomes blocking. When resolved, move the answer to DECISIONS.md and update the relevant doc.

| # | Question | Current placeholder | Blocking at |
|---|---|---|---|
| 1 | Verify against the Rev 3 technical manual: oscillator frequency knob span and stepping (currently ±24 semitones, semitone steps) | as in 01-parameters.md | milestone 4 tuning |
| 2 | Verify: does Osc B with Kbd off also ignore pitch bend and glide? | ignores both | milestone 3 |
| 3 | Verify: first note after idle glides from the voice's last note, or no glide? | glides from last note | milestone 4 |
| 4 | Envelope time ranges (A 1 ms–10 s, D/R 1 ms–15 s): check against hardware measurements if available | P5X values | milestone 4 |
| 5 | LFO shapes: the hardware may allow combining shapes; we use a single choice. Keep radio? | radio | milestone 5 |
| 6 | Add a filter-cutoff destination for aftertouch? | Wheel-Mod amount only | after milestone 6 |
| 7 | Pitch bend range lives in the settings dialog (no panel control). OK? | settings dialog | milestone 5 |
| 8 | Modulation depths (Wheel-Mod ±12 st, Poly-Mod +48 st / +5 oct, filter env +8 oct): tune by ear | as in 05-modulation.md | milestone 4 |
| 9 | Factory preset list: names and categories for the 40 patches | none yet | milestone 6 |
| 14 | The Main panel layout (control positions) only exists in the design canvas on claude.ai, which Claude Code can't open. Ask the user for a starting `art/layout.json` before milestone 5a | none in repo | milestone 5a |
| 13 | Where does the preset **name** show? The chosen look has a 3-digit 7-segment readout, which can only show the number | small light-grey text line under the display | milestone 5 |
| 10 | Milestone 7 SSM2040 mode needs its own spec (filter model + envelope curve differences) before starting | – | milestone 7 |
