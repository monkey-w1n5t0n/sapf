# SAPF (Sound As Pure Form)

## Project Overview
**SAPF** is an interpreter for a mostly functional, stack-based audio synthesis language. It combines the concatenative nature of Forth/Joy with the array-processing power of APL and the lazy evaluation of functional languages. It allows for concise expression of complex audio graphs and synthesis algorithms.

**Key Concepts:**
*   **Language:** Postfix notation, stack-based.
*   **Paradigms:** Concatenative, Functional, Array-oriented (Auto-mapping).
*   **Data Types:** Reals, Strings, Lists (Lazy Streams/Signals), Forms (Dictionaries), Functions.
*   **Architecture:** Virtual Machine (VM) driving Unit Generators (UGens).

## System Requirements & Portability
**⚠️ Current Status: macOS Only**

The project in its current state is heavily dependent on macOS-specific frameworks and libraries:
*   **Accelerate Framework:** Used for DSP (FFT, vector math).
*   **AudioToolbox / CoreAudio:** Used for audio I/O.
*   **Grand Central Dispatch (GCD):** Used for concurrency.
*   **CoreFoundation & IOKit:** Used for system interaction and the Manta controller driver.

**Linux Build Constraints:**
Building this project on Linux is **not currently possible** without significant refactoring to replace these dependencies with cross-platform alternatives (e.g., PortAudio, FFTW/Eigen, std::thread, libusb/hidapi-linux).

## Building (macOS)
The project uses CMake.

```bash
mkdir build && cd build
cmake ..
make -j8
# or
cmake --build . --config Release
```

There is also a `Makefile` wrapper and an Xcode project (`SoundAsPureForm.xcodeproj`).

## Key File Structure

### Source Code (`src/`, `include/`)
*   **`src/main.cpp`**: Application entry point. Sets up the VM, Manta controller, and REPL.
*   **`src/VM.cpp` / `include/VM.hpp`**: The Virtual Machine core. Manages the stack, execution loop, and memory.
*   **`src/UGen.cpp` / `include/UGen.hpp`**: Base class for Unit Generators (audio processing nodes).
*   **`src/Opcode.cpp`**: Implementations of language primitives (stack manipulation, math).
*   **`src/Parser.cpp`**: Text parser for the SAPF language.
*   **`src/dsp.cpp`**: DSP utilities (currently wrappers around Apple's Accelerate framework).

### Documentation & Examples
*   **`README.txt`**: Main documentation covering syntax, types, and usage.
*   **`sapf-examples.txt`**: Extensive collection of code examples demonstrating the language's capabilities.
*   **`sapf-prelude.txt`**: Standard library definitions loaded at startup.

### Dependencies
*   **`libmanta/`**: Library for interfacing with the Snyderphonics Manta controller (currently configured for macOS via `hidapi/mac`).

## Language Syntax Quick Reference
*   **Postfix:** `2 3 +` evaluates to `5`.
*   **Lists:** `[1 2 3]` pushes a list. `#[1 2 3]` pushes a signal/numeric list.
*   **Variables:** `100 = freq` (bind value), `freq` (push value).
*   **Functions:** `\a b [ a b + ]` (defines a function taking `a` and `b`).
*   **Auto-mapping:** `[1 2] 10 +` -> `[11 12]`.
*   **UGens:** `440 0 sinosc` (Sine oscillator at 440Hz).

## Development Conventions
*   **Style:** Tabs for indentation. CamelCase for variables/functions. PascalCase for classes.
*   **Build:** CMake is the source of truth for build configuration.
*   **License:** GPLv3+.
