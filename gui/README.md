# SAPF Experiments — C++ / Dear ImGui

A new implementation of the interaction patterns in James McCartney's 2021
**sapf-ui**, **SAPF2**, and **SoundAsPureForm2** demonstrations. The GUI is written
from scratch; Dear ImGui supplies the immediate-mode widgets and SDL2/OpenGL3
supplies the Linux window. Real sound is evaluated by the existing SAPF engine,
not an approximation of its DSP in the GUI.

## Run on this laptop

```sh
cd ~/src/sapf
./run-experiments --scale 2
```

A **SAPF Experiments** application-menu entry and `sapf-experiments` command are
installed on this laptop with 2x UI scale. `SAPF_UI_SCALE=1.5 sapf-experiments`
overrides it.

The default document is the analog bubbles patch. Click **Play selected**;
change center pitch, lo rate, or hi rate. **Stop / Esc** stops playback.
Expand **implementation** to inspect and edit the nested expression.

`SAPF_ENGINE=/absolute/path/to/run-sapf` selects the interpreter launcher.
Discovery otherwise tries `~/src/sapf-linux/run-sapf`, then `sapf` on PATH.
The editor itself works without an engine; missing-engine errors appear in the
status/console. An engine launcher must set up SAPF's prelude and resource paths,
and must `exec` the interpreter so it follows GUI process lifetime correctly.

## Build

Dependencies: C++17 compiler, CMake, SDL2 development files, OpenGL development
files, and nlohmann-json >= 3.9. Dear ImGui v1.91.9b is pinned and vendored with its
MIT license, so building the GUI does not download anything.

```sh
cmake -S gui -B build/gui -DCMAKE_BUILD_TYPE=Release
cmake --build build/gui -j2
ctest --test-dir build/gui --output-on-failure
```

The original root CMake project remains the macOS SAPF interpreter. The `gui/`
project is independently buildable on Linux. No TZPL/JUCE code is used here.

For live controls, apply [linux-live-controls.patch](patches/linux-live-controls.patch)
to the Linux fork and rebuild it. This is already applied in this laptop's
`~/src/sapf-linux` checkout. It initializes the `zctl` smoothing state and makes
`ZRef` access atomic between the interpreter and audio threads. Other local
Linux-port changes are preserved. See that checkout's `LINUX.md` for its Nix build.

## The reconstructed interactions

- **sapf-ui / prototypes:** nested expression controls; clone any subtree into a
  separate card. Copies have independent values while preserving links inside
  the copied patch.
- **SAPF2 / expressions:** categorized function browser, numeric fields/sliders,
  nested implementation, and per-expression UI/text toggle. Algebraic, Lisp,
  Postfix, Pipeline and Haskell are projections of one expression graph.
- **History:** explicit Commit, Undo, Redo, First/Prev/Next/Last and arbitrary
  checkout. Editing an old revision creates a branch; old futures are retained.
  Expanded nodes, text views, pages, style settings and controls are included.
  Undo first discards uncommitted changes. Clicking another history entry retains
  a dirty draft as a new revision before navigating. Redo chooses the most recent
  child; any alternative is still selectable in the list.
- **UI Style:** light/dark/blue/red themes; slider, number, label, units, tick and
  box visibility; widths, rounding, padding and spacing.
- **Silly & Wobbly Waves:** an adjustable yellow wave graphic, explicitly not
  labeled as an oscilloscope.
- **ImGui / widget experiments:** frequency/duty/phase/amplitude sliders, range
  controls, rotary knobs, ADSR controls, vertical sliders and an envelope view.
  The stock ImGui Demo is a separate optional window.
- **Save/Open:** a JSON document containing the current draft and all history
  branches. Save uses a temporary file and atomic rename. Invalid graphs/history
  are rejected before replacing the current document.

Right-click any expression for clone, clipboard export, replacement or postfix
editing. **Edit** opens a transactional postfix editor for the structural
vocabulary in the function browser (including two-channel lists). Syntax errors
leave the expression unchanged. **Copy SAPF** exports runnable, expanded SAPF.
The **SAPF console** evaluates the full language; **Reset engine** interrupts busy
code and starts a fresh interpreter. Ctrl+S opens Save, Ctrl+Z/Y undo/redo outside
text inputs, and Ctrl+Enter evaluates in the console.

Controls are sent through SAPF's `ZR`/`zctl` mechanism. Live changes preserve
oscillator state. Inputs SAPF requires to be scalar (for example `lftri` initial
phase and comb maximum delay) rebuild the playing patch when changed. Structural
edits rebuild it too. This is not a crossfading live compiler. A final 0.9 clip
bounds extreme output; normal patches should still use sensible gain values.

## Source and fidelity boundary

Reference: [2021 demonstration, 52:48–56:49](https://www.youtube.com/watch?v=fmVdfQNPzkE&t=3168s).
The previous project chat was `01a0c010-8b63-7041-912a-1c7919292913` (2026-09-20).
Its downloaded video, transcript and 17 verified frame captures remain at
`~/research/sapf-gui-2026-09-20/`; `report.html` is the illustrated index.

| Demonstrated evidence | Implementation |
| --- | --- |
| 52:50–53:00 nested controls and copied subexpressions | Prototype cards, numeric inputs, structural clone |
| 53:35 bubbles parameters | Same 81 / 0.4 / 8 defaults, real SAPF analog-bubbles expression |
| 54:05 implementation drill-down | Expand the bubbles implementation into oscillators, then phase accumulators, arithmetic, stereo rates and comb delay |
| 54:19–54:30 five syntax names | Five text projections of the same graph |
| 55:15–55:20 retained history and opening/undo | Explicit commits, branching snapshots, arbitrary checkout |
| 55:40 waveform and dimensions | Adjustable wave drawing and layout settings |
| 56:00 red theme | Four switchable themes |
| 56:30–56:45 custom widgets | Sliders, ranges, knobs, vertical sliders, ADSR/envelope |

This is a behavioral reconstruction from the video, not recovered private source
or a pixel-identical copy. The structural browser currently contains the 19
functions used to construct and explore these examples, not the entire SAPF
language. Bubbles, sinosc, lfsaw, nnhz and bipolar conversion have editable mathematical
implementations, down to phase accumulation and arithmetic. These are reconstructed
decompositions, not recovered private SAPF2 definitions. `phac` is lowered to SAPF
using a fixed-phase native saw; other native leaves show their role and documentation. Exact private SAPF2 syntax
semantics and undocumented mouse gestures cannot be inferred from the recording.
The widget panel is a widget experiment, as demonstrated; it is not falsely
presented as a playable instrument. The bubbles patch and structural expressions
are the playable part.

The convenience of persistent files and the full-language console are additions.
The video's proposed automatic commit behavior was a future intention, so explicit
Commit remains. The widget panel's shared vertical/horizontal values and envelope
preview are useful reconstructions; the video does not establish exact behavior
for every hidden widget.

## Verification

```sh
ctest --test-dir build/gui --output-on-failure
python3 gui/tests/gui_smoke.py
python3 gui/tests/audio_reference.py
```

The GUI test requires `pactl` and `parec`; the offline reference test requires
`ffmpeg`. The GUI check opens a real SDL/OpenGL window, injects mouse/key events through
ImGui IO to exercise the actual controls, and captures real SAPF output from an
isolated PipeWire/Pulse sink. It checks history branching, cloning, all syntax
selectors, widget interaction, save/open, a 440 Hz tone, live change to 880 Hz
without restarting playback, Stop to silence, and console/reset. Test mode is
explicit (`--probe`, `--test-input`) and does not run in normal launches.

Original project license: GPL-3.0-or-later; Dear ImGui has its own MIT license.

Verified on 2026-09-20: 90 model checks; native GUI/audio interaction suite; all
29 Linux engine C++ tests (9,974,926 assertions). The reconstructed bubbles graph
matches 48,000 stereo frames of the original example to a maximum sample
difference of 3.7252903e-09. Logs and captures are in `build/verification/`.
