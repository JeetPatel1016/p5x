# 13 · Art pipeline (realistic UI assets)

The panel looks like real hardware. All shipped art is **rendered in Blender from scripts in this repo**, so the author owns it, it's reproducible at any resolution, and every knob and label lines up with the code.

## Rules
- **AI-generated concept images are inspiration only.** They live outside the code repo in `../artwork/concepts/` (the `artwork` folder next to `dev`) and are never shipped, traced or used as textures. Reasons: ownership of AI images is legally unclear (a problem if P5X goes paid), and an image model can't produce exact, consistent rotation frames.
- Don't reproduce the original hardware's panel graphics, logo, lettering or exact layout (trade dress). Similar *materials* are fine.
- Textures: only CC0 sources (Poly Haven, ambientCG) or procedural Blender materials. List each texture in THIRD_PARTY.md with its source URL.
- Blender is only a tool; its GPL doesn't apply to renders.

## Single source of truth: `art/layout.json`
Every control's position lives in one file, used by **both** the Blender panel script (to place printed labels, scales and cutouts) and the C++ UI (to place components). Nothing is positioned by hand in two places.
```json
{
  "base": { "w": 1440, "h": 600 },
  "controls": [
    { "id": "flt_cutoff", "type": "knob", "x": 612, "y": 318, "label": "CUTOFF" },
    { "id": "osc_a_saw", "type": "button", "x": 980, "y": 110, "label": "SAW" }
  ],
  "areas": [
    { "id": "preset_display", "x": 520, "y": 14, "w": 280, "h": 36 },
    { "id": "keyboard", "x": 150, "y": 490, "w": 1210, "h": 100 }
  ],
  "sections": [ { "title": "FILTER", "x": 560, "y": 240, "w": 330, "h": 150 } ]
}
```
- `x`, `y` = control centre in base (1× @ 100 %) pixels.
- CMake copies `layout.json` into BinaryData; the editor reads it at construction. A unit test checks every parameter in 01-parameters.md (except `bend_range`) appears exactly once in `controls`, and no unknown IDs appear.
- Starting positions come from the Main panel artboard, which isn't in the repo (see OPEN_QUESTIONS #14): ask the user for a starting `layout.json` before milestone 5a. After that, adjust positions in `layout.json` only.

## Asset list
| Asset | Frames / states | @1x size | @2x size | File |
|---|---|---|---|---|
| Panel background (incl. wood cheeks, printed labels, section lines, knob scales, display window) | 1 | 1440 × 600 | 2880 × 1200 | `panel.png` (opaque) |
| Knob, standard | 128 frames, −135° → +135° | 60 × 60 per frame | 120 × 120 | `knob.png` (vertical strip, transparent) |
| Knob, large (Filter Cutoff only) | 128 frames, −135° → +135° | 76 × 76 per frame | 152 × 152 | `knob_large.png` |
| Button (square, whole cap backlit, no text) | 4: off, on, off-pressed, on-pressed | 34 × 24 | 68 × 48 | `button.png` (vertical strip, transparent) |
| Pitch wheel | 65 frames, full down → full up | 26 × 70 | 52 × 140 | `wheel_pitch.png` |
| Mod wheel | 65 frames, 0 → max | 26 × 70 | 52 × 140 | `wheel_mod.png` |
| White key | 2: up, down | 55 × 100 | 110 × 200 | `key_white.png` |
| Black key | 2: up, down | 30 × 62 | 60 × 124 | `key_black.png` |
| Debug / Settings windows | – | – | – | stay code-drawn (flat), no art |

Output folder: `Resources/ui/1x/` and `Resources/ui/2x/`. Rendered PNGs are **committed** (CI doesn't run Blender). Budget: all UI assets together < 25 MB.

## Blender setup
- Blender 4.2 LTS, run headless: `blender -b -P art/scripts/render_all.py -- --scale 2 --out Resources/ui/2x`.
- Scripts: `render_panel.py`, `render_knob.py`, `render_button.py`, `render_wheels.py`, `render_keys.py`, `render_all.py`, shared `style.py` (materials, lights, camera). All geometry and materials are built by code; no hand-edited `.blend` files committed (a generated `.blend` may be saved for preview but isn't a source).
- **Camera:** orthographic, straight top-down, so knob frames composite exactly onto the panel. Same camera settings for every asset; ortho scale derived from pixel size.
- **Lighting (identical for all assets):** one large soft area light from the top-left at 45°, plus a dim fill from the right, plus a subtle environment (CC0 studio HDRI). Consistent lighting is what makes separate renders look like one object.
- **Shadows:** knobs, buttons and wheels render on a transparent background with their contact shadow included (shadow catcher). The panel renders without them.
- **Render:** Cycles, fixed seed, 256 samples + denoiser, 16-bit PNG then quantized to 8-bit with dithering. Colour management: AgX, same look for all.
- **Printed text:** labels, section titles and scale ticks are generated as Blender text objects in Kode Mono (uppercase, from `Resources/fonts/KodeMono/`) from `layout.json`, so they're part of the panel render (silk-screen look, slight ink texture).

## Style (locked)
The look is **Direction C**, reference image `../artwork/concepts/direction-3.jpg`. `art/style.json` holds the materials, colours and details taken from it: graphite bead-blasted anodized panel, light oak cheeks, machined aluminium knobs (one large knob for Filter Cutoff), square buttons whose whole cap glows warm amber, a 3-digit 7-segment preset readout, light-grey uppercase Kode Mono lettering, rounded section outlines with titles on the top edge.

- Match the reference by eye; the sampled hex values are starting points.
- **Only the look comes from the reference, not its controls.** The image model invented its own knob and button labels and layout; ignore them. Controls, labels and positions come from `layout.json` (01-parameters.md + the Main panel artboard).
- Changing `style.json` needs the user's approval and a DECISIONS.md line.

## Code side
- `ui/ImageKnob` (filmstrip), `ui/ImageButton4` (4-state), `ui/ImageWheel`, `ui/RealKeyboard`: all read their frames from the loaded strips; no drawing besides the image.
- The @1x/@2x choice and resampling rules are in 08-ui.md § Tech.

## Tests / acceptance
- `render_all.py` is deterministic: two runs produce identical PNG hashes (fixed seed, no timestamps in metadata).
- Filmstrip frame count and dimensions match the table (unit test reads the PNG headers).
- Layout test above (every param has exactly one control).
- Manual: knob at 0 / 50 / 100 % lines up with the printed scale; lighting looks consistent between panel, knobs and buttons.
