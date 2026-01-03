# Agent Manifest

This document serves as the primary specification for the AI agent to implement the MusicVisVibeCode project.

## Agent Notes
- Always rebuild on code updates.
- Always use CMake found here: `"C:\Program Files\CMake\bin;"`
- Always sync from updated `Readme.md` files to `Manifest.md` files in same directory.

## Project Overview
A music visualization application using C++, WASAPI for audio capture, and DirectX for rendering.

## Technology Stack
- **Language**: C++
- **Audio Capture**: Windows Audio Session API (WASAPI)
- **Graphics**: DirectX (Version 11 or 12 recommended)
- **Build System**: CMake (Recommended for VS Code integration)

## Architecture

### 1. Audio Analyzing Engine
**Responsibility**: Capture system audio, process FFT, and expose data to visualizations.
**Performance**: Sample audio at 60Hz.

**Data Structure**:
The engine must expose a struct/class with the following accessible data:

- `bool playing`: True if audio is detected, False if no audio for > 3 seconds.
- `float Spectrum[256]`: Current spectrum values (0.0 - 1.0).
- `float History[60][256]`: Circular buffer of last 60 spectrum frames.
  - `History[0]` is the most recent.
  - `History[59]` is the oldest.
  - Implementation should use an index offset rather than shifting array values.
- `float Scale`: Volume scale factor (0.0 - 1.0) for normalization.
- `float SpectrumNormalized[256]`: `Spectrum` values normalized by `Scale`.
- `float HistoryNormalized[60][256]`: `History` values normalized by `Scale`.
- `float SpectrumHighestSample[256]`: For each frequency bin, the maximum value from the last 6 frames of `HistoryNormalized`.

**Scaling Logic (AGC)**:
- **Expansion (Immediate)**: If any value in the raw `Spectrum` array exceeds the current `Scale` (ceiling), immediately set `Scale` to that new peak value.
- **Contraction (Gradual)**: If the maximum value of the current `Spectrum` is less than the current `Scale`, reduce `Scale` by 5% per second.
- **Floor**: Define a `MIN_SCALE` (e.g., 0.001) to prevent the scale from dropping to zero.
- **Note**: `Scale` variable in `AudioData` represents the "Ceiling" or "Max Value". Normalization should be `Raw / Scale`.

### 2. Visualization Interface
Visualizations should consume the data structure provided by the Audio Engine.

#### Visualization: Spectrum
See `Manifest/Visualizations/Spectrum/Manifest.md` for detailed specifications.

#### Backgrounds
- Select randomly 1 picture from `Backgrounds/` folder.
- Image should be smoothly zoomed to fill screen (cover mode), no black bars.
- Do not stretch; scale and crop as necessary.

## Controls (Global)
- `H`: Toggle Help Menu (Hides Info/Clock if shown).
- `I`: Toggle Info Overlay (Hides Help/Clock if shown).
- `C`: Toggle Clock (Hides Help/Info if shown).
- `N`: Toggle between Normalized and Raw values.
- `F`: Toggle Fullscreen.
- `R`: Select Random Visualization.
- `B`: Change Background Randomly (ensure new image, do not toggle off).
- `[`: Previous Background (wrap around).
- `]`: Next Background (wrap around).
- `Left Arrow`: Previous Visualization.
- `Right Arrow`: Next Visualization.
- `1-0`: Select Visualization 1-10.
- `ESC`: Quit application.

## OSD (On-Screen Display)
- **Position**: Top Right.
- **Style**: Slightly tinted transparent box behind text for readability.
- **Help Menu**: Displays list of keyboard shortcuts.
- **Info Overlay**: Displays debug info (FPS, Decay Rate, Audio Scale, Playing Status, Current Vis Name).
- **Clock**: Digital style clock.
  - **Font**: Large.
  - **Alignment**: Right aligned.
  - **Background**: Black tinted box (~50% transparency), 20% wider and taller than text.
  - **Format**:
    ```
    HH:MM:SS
    DD/MM/YYYY
    Day Of Week
    ```
  - **Note**: Clock should be its own box and font size and not shared with Info and Help OSD.
- **Behavior**: Help, Info, and Clock are mutually exclusive.

## Startup Behavior
- Always load a random background if available.

## Implementation Plan
1.  **Setup**: Initialize C++ project with CMake and dependencies.
2.  **Audio Engine**: Implement WASAPI capture and FFT processing.
3.  **Rendering**: Setup DirectX window and render loop.
4.  **Data Binding**: Connect Audio Engine data to Rendering engine.
5.  **Spectrum Vis**: Implement the specific rendering logic for the Spectrum visualization.
