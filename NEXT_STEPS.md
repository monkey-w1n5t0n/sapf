# Next Steps for SAPF Linux Port

The project has been refactored to compile and run the REPL on Linux. Basic audio output and synthesis work using `miniaudio` and `pocketfft`. However, several platform-specific features were stubbed to achieve the initial build.

The ultimate goal is **100% feature parity** with the macOS version.

## 1. Audio File I/O (Critical)
**Current Status:** Stubbed in `src/SoundFiles.cpp`.
**Goal:** Implement `sfread`, `sfwrite`, and `sfcreate`.
**Recommended Approach:**
*   Use **`libsndfile`** or `dr_wav` / `dr_flac` (from the `dr_libs` collection, similar style to `miniaudio`).
*   **`sfread`:** Needs to load audio data into the `SFReader` buffer.
*   **`sfwrite` / `recordWithAudioUnit`:** Needs to write the audio stream to disk. `miniaudio` has encoding APIs, or use `libsndfile`.

## 2. MIDI Support (High)
**Current Status:** Stubbed in `src/Midi.cpp`. All MIDI Ops (`midiStart`, `mctl`, `noteOn`, etc.) currently do nothing or print "Not implemented".
**Goal:** Full MIDI input and output support.
**Recommended Approach:**
*   Replace the CoreMidi dependency with **`RtMidi`** (a set of C++ classes that provides a common API for realtime MIDI input/output across Linux (ALSA/JACK), macOS, and Windows).
*   Refactor `src/Midi.cpp` to wrap `RtMidi` calls instead of CoreMidi calls.
*   Ensure the `MidiChanState` global struct is updated by the `RtMidi` callback.

## 3. Mouse Input (Medium)
**Current Status:** Stubbed in `src/UGen.cpp`. `MouseX` and `MouseY` UGens return static values (center of screen).
**Goal:** Read global mouse coordinates for control rate modulation.
**Recommended Approach:**
*   Since SAPF is a CLI tool, it doesn't own a window. You must query the global pointer position.
*   **X11:** Use `XQueryPointer` from `X11/Xlib.h`.
*   **Wayland:** This is harder due to security isolation. You might need to rely on `/dev/input/event*` (requires permissions) or just support X11 via XWayland.
*   **Implementation:** Update `gstate_update_func` in `src/UGen.cpp`.

## 4. DSP Optimization (Medium)
**Current Status:** `vDSP` functions (Apple Accelerate) are shimmed in `include/vDSP_shim.hpp` using standard C++ loops and `pocketfft`.
**Goal:** Restore performance parity.
**Recommended Approach:**
*   The current shim is functional but likely slower than the vectorized Accelerate framework.
*   Use a SIMD library like **`xsimd`** or **`Vc`**, or compiler auto-vectorization hints (`#pragma omp simd` if OpenMP is an option, or just careful loop construction).
*   Focus on `vDSP_vaddD`, `vDSP_vmulD`, and the `vv*` (vForce) math functions (`vvsin`, `vvexp`, etc.).

## 5. Image Generation (Low)
**Current Status:** Replaced Objective-C `NSBitmapImageRep` with `stb_image_write` in `src/makeImage.cpp`.
**Goal:** Verify feature completeness.
**Verification:**
*   Check if `src/Spectrogram.cpp` functionality (generating spectrogram images) works as expected.
*   The current implementation supports PNG. Ensure this meets the user's needs (original might have supported TIFF/JPEG).

## 6. HIDAPI / Manta (Low)
**Current Status:** Configured to use `libusb` backend.
**Goal:** Verify Manta controller support.
**Verification:**
*   If a Manta controller is available, test connection.
*   Ensure udev rules are set up correctly on the Linux host to allow the user to access the USB device.

## 7. Code Cleanup
*   **Preprocessor Directives:** There are many `#ifdef __APPLE__` blocks. Over time, abstract these into a `Platform` class or separate source files (e.g., `Midi_CoreMidi.cpp`, `Midi_RtMidi.cpp`) to keep the core logic clean.
*   **Types:** Consolidate the type shims (like `OSStatus`, `UInt32`) into a common platform header.

## Summary of Libraries needed:
*   **Audio File:** `libsndfile` or `dr_libs`
*   **MIDI:** `RtMidi`
*   **Mouse:** `libX11`
*   **SIMD:** `xsimd` (Optional but recommended)
