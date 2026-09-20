# Verification — 2026-09-20, blooper / Linux

- CMake GCC 16.2.1 build, SDL2 2.32.72, Dear ImGui v1.91.9b, OpenGL3.
- `model-tests`: **90 checks pass**. Shared input links survive cloning without
  aliasing the original; all syntax projections; invalid edit atomicity; history
  forks and arbitrary checkout; expanded UI state; save/load; invalid graph,
  depth, cycle and history rejection.
- `gui/tests/gui_smoke.py`: **pass** in a real SDL/OpenGL window, ImGui mouse/key
  event injection. Numeric entry, implementation expansion, manual commit,
  undo/branch checkout, independent clone, five syntax choices, text/UI toggle,
  all four pages, widget slider/knob, Save/Open, console evaluation and Reset.
- GUI Play -> actual SAPF -> isolated PipeWire monitor: **440 Hz at peak 0.04**;
  live numeric change -> **880 Hz at peak 0.04**, with no second play command;
  Stop -> **zero** captured samples. The test removes its sink and interpreter.
- `gui/tests/audio_reference.py`: **48,000 stereo frames** from the expanded
  default graph versus the original `sapf-examples.txt` analog-bubbles expression.
  Maximum absolute sample difference **3.7252903e-09**, peak **0.072287969**.
  The initialized live control gives a 1,024-sample mean of exactly 440.
- Linux backend after live-control patch: **29 C++ cases / 9,974,926 assertions**
  pass. Patch changes only Object.hpp, Object.cpp and Midi.cpp; inherited local
  Linux usability changes were preserved. Patch is included under `gui/patches/`.

Generated logs, probe output and native screenshots live in `build/verification/`
(ignored build artifacts). The tests intentionally do not infer physical-speaker
listening, MIDI hardware behavior, undocumented private prototype behavior or
bit-for-bit equivalence of all possible programs from these results.
