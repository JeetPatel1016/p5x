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
| 2026-09-29 | Pinned JUCE 8.0.15 (latest 8.x tag) and Catch2 v3.16.0, fetched as release archives with SHA-256 hashes | Reproducible, fast builds |
| 2026-09-29 | Milestone 1 placeholder voice: sine, fixed linear envelope (5 ms attack, 100 ms release), voice gain 0.3, pitch = note + bend + master tune, `master_volume` applied, velocity ignored, no clipper, no oversampling (latency 0), `perf_voices` honoured | Envelopes and output stage arrive in milestone 2; M1 still needs click-free notes and a Release stage for the steal order |
| 2026-09-29 | Audio device types: ASIO plus every Windows type JUCE offers (Windows Audio shared / Exclusive / Low Latency, DirectSound). ASIO SDK used under the GPLv3 option of Steinberg's dual license (since Oct 2025), fetched at build time, never committed; a paid release needs Steinberg's proprietary ASIO license | User wants multiple interface options; personal/open-source use for now |
| 2026-09-29 | Logger uses a from-scratch lock-free bounded multi-producer ring instead of `AbstractFifo` for non-message threads | `P5X_LOG` is callable from any thread, and several instances or host threads can log at once; `AbstractFifo` is single-producer |
| 2026-09-29 | `dsp/` never logs; events such as voice steals are returned to the processor, which logs them | Keeps `p5x_dsp` standard-library-only |
| 2026-09-29 | `p5x_midi` and `p5x_debug_core` are CMake INTERFACE libraries; the dependency boundary is enforced by `p5x_tests` linking only their allowed JUCE modules | JUCE modules compile into each binary that links them; two static libraries both containing JUCE would clash |
| 2026-09-29 | Until the artboards are provided, the Settings dialog and Debug console use a plain functional layout (palette + Kode Mono); `direction-3.jpg` is the visual inspiration | Artboards aren't in the repo |
| 2026-09-29 | Standalone `Route input into filter` deferred to milestone 2 | There's no filter until milestone 2 |
| 2026-09-29 | Program Change is parsed and logged only until presets exist (milestone 6) | No preset list before milestone 6 |
| 2026-09-29 | Versioning: milestone N ships as 0.N.0 (milestone 1 = 0.1.0) | Clarifies "bump the minor version" |
| 2026-09-29 | Computer keyboard default: the A key plays C3 (MIDI 48) | Middle octave of the on-screen C2–B4 keyboard |
| 2026-09-29 | `settings.xml` is rewritten on every Settings dialog change | Spec didn't say when it's written |
| 2026-09-29 | Doc consistency pass: fixed arithmetic in the LFO square slew test (bound is 2/(0.001 × rate), not 1/…); aligned 08-ui.md with the locked Direction C look (4-state backlit buttons, light oak cheeks); aligned the 00 module map with 13's UI class names; clarified velocity scaling of the filter envelope, pitch formulas, frequency clamps, `<MidiMap>` precedence, debug console staging | Docs contradicted each other or the locked decisions; no behaviour choices were made (those went to OPEN_QUESTIONS #15–#35) |
| 2026-09-29 | P5X plays 61 keys: MIDI notes 36–96 (C2–C7). Notes outside are ignored (shown dimmed in the MIDI monitor). The on-screen keyboard shows all 61 keys and never shifts; key art shrinks to 33 × 100 (white) / 20 × 62 (black) @1x so 36 white keys fit in 1188 px. The computer keyboard's A key moves C2–C6. Supersedes "notes 0–127 accepted", the 3-octave on-screen keyboard, and the C0–C7 computer keyboard range (resolves OPEN_QUESTIONS #16) | User's choice: 61-key support, on-screen and note range |
| 2026-09-29 | Pickup takeover applies to Float and Int params only; Bool params always toggle on the rising edge, Choice params always jump (resolves #17) | A toggle has no value to cross |
| 2026-09-29 | Mapped-control tooltip uses 08's format with real units: `Cutoff · 1850 Hz · CC 74` (resolves #18) | One format everywhere |
| 2026-09-29 | Log entries carry `instanceId` (0 = global) and a defined `source` subsystem; the logger is process-wide and each console shows its own instance plus global entries (resolves #19) | Several instances share one process and log |
| 2026-09-29 | State `version` missing, non-numeric or < 1 → defaults; newer → parsed tolerantly with a WARN (resolves #20) | Don't throw away a newer project's settings |
| 2026-09-29 | Robustness test: 5 s of audio per rate × block-size case in every build (supersedes 60 s; resolves OPEN_QUESTIONS #36) | 60 s per case (~20 min of audio) is too slow for routine runs |
| 2026-09-29 | 0.1.1: voice gain 0.3 → 0.2 (06-voices.md); supersedes the 0.3 in the milestone 1 placeholder voice line | Measured: 5-note chords at 0.3 peaked at +2.6 to +3.4 dBFS and hard-clipped at the sound card; at 0.2 they peak at −1.0 to −0.5 dBFS |
| 2026-09-29 | 0.1.1: safety clipper pulled forward from milestone 2, formula `y = sign(x)·(0.8 + 0.2·tanh((abs(x) − 0.8)/0.2))` above 0.8 (resolves OPEN_QUESTIONS #27; supersedes "no clipper" in the milestone 1 placeholder voice line) | The spec's formula wasn't continuous; 8–10 voices still exceed 0 dBFS at gain 0.2 and must not hard-clip |
| 2026-09-29 | Versioning: fixes between milestones bump the patch version (0.1.1, 0.1.2, …); milestone N still ships as 0.N.0 | User asked for the clipping fix as 0.1.1 |
| 2026-09-29 | 0.1.1: voice gain 0.2 → 0.16 (supersedes the 0.2 line above) | At 0.2, 5-voice chords peaked at 0.98 and the safety clipper's knee added faintly audible distortion (~50 dB down, in bursts) on the M1 sines; at 0.16 five voices sum to at most 0.8, so the clipper never engages with the default voice count |
| 2026-09-29 | Specs come first, tests follow: when a test disagrees with the specified behaviour or model, the test changes (recorded here), never the spec (CLAUDE.md working rule 2) | User's rule |
| 2026-09-29 | Filter tests rewritten to match the 03-filter.md model: passband loss at res 0.9 is 13.1 dB ± 1 (was 3–8 dB); 2–4 kHz slope is 21.2 dB ± 1 (was 24 ± 2); self-oscillation RMS measured 8–9 s (was 1–2 s; start-up at 100 Hz takes ~4.4 s); sweep test defined as one full 20 Hz ↔ 20 kHz sweep per 1 ms with noise input. Resolves OPEN_QUESTIONS #21, #22, #23, #25 | Values computed from the model; tests follow the spec |
| 2026-09-29 | Saw aliasing test (02-oscillators.md): fundamentals 1 kHz to C8; threshold set in milestone 2 from what the specified PolyBLEP-at-2× model measures, recorded here then (−80 dB was an estimate). Resolves OPEN_QUESTIONS #24 | Tests follow the spec |
