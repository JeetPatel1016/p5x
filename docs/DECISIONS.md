# Decisions log

Append-only. Each entry: date · decision · reason. Changing a decision means adding a new entry that supersedes the old one.

| Date | Decision | Reason |
|---|---|---|
| 2026-09-29 | Model Rev 3 (CEM3340/3320/3310) first; Rev 1/2 SSM2040 filter is optional milestone 7 | Best-documented revision; fastest path to a good sound |
| 2026-09-29 | Open source (GPL-3.0) now, author keeps 100 % copyright; may relicense/sell later | Personal use first; keep the paid option open |
| 2026-09-29 | Write all code from scratch; no code from GPL/AGPL/LGPL projects | Required for relicensing |
| 2026-09-29 | Windows only, VST3 + Standalone | Author's platform; fastest to ship |
| 2026-09-29 | Faithful + light extras (velocity, aftertouch, unison, glide, Vintage); no FX | Keep scope tight |
| 2026-09-29 | Full MIDI from milestone 1; Standalone is the test harness with device config, keyboards, debug console | Testing must be painless |
| 2026-09-29 | UI: hybrid look, JUCE vector drawing, locked to the P5X Panel canvas | Scales cleanly, easy to build, avoids trade dress issues |
| 2026-09-29 | No default CC map; Kontakt-style right-click MIDI Learn only (supersedes the earlier KeyLab default profile) | Works with any controller; user controls the layout |
| 2026-09-29 | Reserved CCs: 1 (Wheel-Mod), 64 (sustain), 120–127 | Hardwired performance controls and channel mode messages |
| 2026-09-29 | Aftertouch adds to Wheel-Mod amount only | Matches modern practice; filter destination deferred (OPEN_QUESTIONS) |
| 2026-09-29 | Voices sum at the oversampled rate and downsample once | CPU: one downsampler instead of one per voice |
| 2026-09-29 | Mappings and settings belong to the instance, not presets | Changing presets must never break the controller setup |
| 2026-09-29 | All randomness seeded and stored in state | Projects sound identical on reload; tests are deterministic |
| 2026-09-29 | UI switches to a realistic hardware look drawn from pre-rendered images (filmstrip knobs, photoreal panel) in plain JUCE; supersedes "hybrid look, JUCE vector drawing". No WebView | User found the vector look flat; realism comes from art, not rendering tech |
| 2026-09-29 | All shipped art rendered from Blender scripts in the repo; AI concept images are inspiration only | Author owns the art; reproducible at any scale; AI image ownership is unclear |
| 2026-09-29 | `art/layout.json` is the single source of control positions for both renders and code | Prevents knobs and printed labels drifting apart |
| 2026-09-29 | Debug console and Settings dialog stay flat and code-drawn | Tools, not instrument; realism adds nothing |
| 2026-09-29 | Look locked to Direction C (`../artwork/concepts/direction-3.jpg`): graphite panel, light oak cheeks, aluminium knobs, amber backlit square buttons, 7-segment preset readout. Details in `art/style.json` (resolves OPEN_QUESTIONS #11). Modern directions M1–M4 were explored and rejected | User preferred it over all other concepts |
| 2026-09-29 | Button labels are printed on the panel below the buttons, not inside them (resolves OPEN_QUESTIONS #12) | Matches the chosen reference and real hardware |
| 2026-09-29 | Panel lettering: IBM Plex Sans, regular width, sentence case (replaces Plex Sans Condensed, uppercase) | Matches the chosen reference |
| 2026-09-29 | Font: Kode Mono (SIL OFL) in uppercase everywhere, replacing IBM Plex entirely (supersedes the line above) | User's choice |
| 2026-09-29 | Author and vendor: Jeet Patel. Manufacturer code `JtPl`, plugin code `P5xS`, bundle ID `com.jeetpatel.p5x` (permanent) | User's choice; shown in DAW plugin info |
| 2026-09-29 | pluginval v1.0.4 lives in `Desktop\p5x\tools\pluginval\`, outside the repo | GPL tool; never part of the codebase |
| 2026-09-29 | Filter Cutoff gets a larger knob | In the chosen reference; it's the most-played control |
