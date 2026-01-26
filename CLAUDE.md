# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MusicVisVibeCode is a real-time music visualization application built with C++, using WASAPI for audio capture and DirectX 11 for rendering. The project is designed for "vibe coding" with AI agents - see `Manifest/` directory for detailed specifications.

## Build System

This project uses CMake with MinGW on Windows.

**CRITICAL**: Always use CMake from `C:\Program Files\CMake\bin\` - NEVER use CMake under `C:\Strawberry`.

### Build Commands

```bash
# Kill existing process if running (prevents file lock errors)
taskkill /F /IM MusicVisVibeCode.exe 2>/dev/null || true

# Clean build (recommended after code changes)
rm -rf build && mkdir build && cd build
"C:\Program Files\CMake\bin\cmake.exe" -G "MinGW Makefiles" ..
mingw32-make

# Incremental build (faster when only source files changed)
cd build
mingw32-make

# Sync executable to root (for GitHub publishing)
cp ./build/MusicVisVibeCode.exe ./MusicVisVibeCode.exe
```

**Always rebuild after code updates** - the Manifest files explicitly require this.

**Note**: Use `taskkill` before rebuilding to ensure the executable isn't locked by a running process.

### Testing

Test builds using CLI arguments to verify the executable doesn't crash:

```bash
# Test specific visualization with timeout
./MusicVisVibeCode.exe --vis <name> --timeout 5

# Test with snapshot for visual debugging (saves snapshot.png after 2 seconds)
./MusicVisVibeCode.exe --vis cybervalley2 --snapshot 2 --timeout 5

# Available visualizations
./MusicVisVibeCode.exe --vis spectrum --timeout 5
./MusicVisVibeCode.exe --vis cybervalley2 --timeout 5
./MusicVisVibeCode.exe --vis linefader --timeout 5
./MusicVisVibeCode.exe --vis spectrum2 --timeout 5
./MusicVisVibeCode.exe --vis circle --timeout 5

# Get help
./MusicVisVibeCode.exe --help
```

**IMPORTANT**: When testing new visualizations, use the visualization NAME (e.g., "linefader") not its number. Always include the `--timeout` option.

**Snapshot Feature**: Use `--snapshot <seconds>` to automatically save a PNG screenshot after N seconds. This is useful for debugging visual updates without manually taking screenshots. The snapshot time should be shorter than the timeout value.

## Architecture

### Three-Layer Architecture

1. **Audio Engine** (`src/audio/AudioEngine.{h,cpp}`)
   - Captures system audio via WASAPI
   - Performs FFT analysis at 60Hz
   - Exposes `AudioData` struct with spectrum data
   - Runs on separate thread

2. **Renderer** (`src/rendering/Renderer.{h,cpp}`)
   - DirectX 11 window management and render loop
   - Background image loading (GDI+)
   - OSD rendering (help, info, clock)
   - Input handling and visualization switching
   - Config persistence

3. **Visualizations** (`src/visualizations/`)
   - Inherit from `BaseVisualization`
   - Self-contained rendering logic
   - Per-visualization settings and controls
   - Each has corresponding spec in `Manifest/Visualizations/<name>/`

### AudioData Structure

The `AudioData` struct is the contract between audio engine and visualizations:

- `bool playing` - Audio detected in last 3 seconds
- `float Spectrum[256]` - Raw spectrum values (0-1)
- `float History[60][256]` - Circular buffer of last 60 frames
- `float Scale` - Current AGC ceiling value
- `float SpectrumNormalized[256]` - Spectrum / Scale
- `float HistoryNormalized[60][256]` - History / Scale
- `float SpectrumHighestSample[256]` - Max of last 6 frames per bin

**Key Detail**: `History` is a circular buffer using `historyIndex` offset - values are NOT shifted.

**AGC (Automatic Gain Control)**:
- **Expansion**: If any spectrum value exceeds `Scale`, immediately set `Scale` to that peak
- **Contraction**: If peak < `Scale`, reduce `Scale` by 5% per second
- **Floor**: Minimum scale is 0.001
- **Ceiling**: Maximum scale is 1.5 (enforced by capping peaks to 0.667)

### Config System

`Config` class (`src/Config.{h,cpp}`) persists settings to `~/.musicvibecode/config.txt`:
- Main application state (fullscreen, background, current vis)
- Per-visualization settings
- Enabled/disabled visualizations
- Auto-saves when dirty after delay

## Adding New Visualizations

1. Create directory in `Manifest/Visualizations/<name>/` with `Readme.md` and `Manifest.md`
2. Implement `src/visualizations/<Name>Vis.{h,cpp}` inheriting from `BaseVisualization`
3. Add to `Renderer.h` enum and `m_visualizations` array
4. Update `main.cpp` CLI parser with visualization name/aliases
5. Register in `Renderer.cpp` initialization
6. Add config fields to `Config.h` if needed
7. Test with `--vis <name> --timeout 5`

## Manifest System

The `Manifest/` directory contains specification files used to guide AI agents:

- `Manifest/Readme.md` - Human-written high-level notes
- `Manifest/Manifest.md` - Agent-derived detailed specifications
- `Manifest/Visualizations/<name>/Readme.md` - Per-vis human notes
- `Manifest/Visualizations/<name>/Manifest.md` - Per-vis agent specs

**Agent Workflow**: Read `Readme.md` files → Sync changes to `Manifest.md` files → Implement code.

## Key Controls

Global (defined in `Manifest/Manifest.md`):
- `H` - Toggle help overlay
- `I` - Toggle info overlay (FPS, scale, vis name)
- `C` - Toggle clock
- `F` - Toggle fullscreen
- `R` - Random visualization
- `B` - Random background (from `Backgrounds/`)
- `[` / `]` - Previous/next background
- `Left` / `Right` - Previous/next visualization
- `1-0` - Select visualization 1-10
- `ESC` - Quit

Each visualization can define additional controls shown in help overlay.

## DirectX Resources

The renderer manages shared DirectX resources:
- `m_device`, `m_context` - D3D11 device and context
- `m_vertexShader`, `m_pixelShader` - Shared shaders
- `m_inputLayout` - Vertex input layout
- `m_vertexBuffer` - Dynamic vertex buffer (updated per-frame by visualizations)

Visualizations receive these resources in their `Update()` method and write vertices to the shared buffer.

## Background Images

- Loaded from `Backgrounds/` directory
- Scaled to cover screen (cropped, not stretched)
- Selected randomly on startup if available
- Managed by `Renderer` using GDI+ for image loading

## OSD (On-Screen Display)

Three mutually exclusive overlays (Help, Info, Clock):
- Positioned top-right
- Rendered to texture using GDI+, then drawn as overlay
- Semi-transparent black background box
- Clock has separate texture/resources with larger font

## Important Notes

- Always prefix Bash commands with quotes for paths containing spaces
- The executable is in both root and `build/` after compilation
- Use visualization names (not numbers) in CLI and code references
- Circular buffers use index offsets, not array shifting
- Config auto-saves but can be manually triggered

## Recent Visual Updates (CyberValley2)

### 2026-01-26 - CyberValley2 Enhancements

**Sun/Moon Improvements:**
- **Size**: Doubled from radius 0.15 to 0.30 for more prominent celestial body
- **Position**: Repositioned so bottom 1/5th is below horizon (partially setting/rising)
- **Stripes**: Changed from dark overlays to transparent "cut-out" gaps showing sky through
  - Uses sky gradient color to create authentic vaporwave aesthetic
  - 5 horizontal stripes across lower 40% of orb
  - Creates classic retro/outrun sun appearance with visible gaps between slices

**Road Enhancements:**
- **Edge Lines**: Added white lines at outer road boundaries (left and right edges)
  - Slightly thicker (0.004 units) than center lines
  - Defines road boundaries and enhances perspective depth
- **Grid Overlay**: Softened to 30% opacity for subtle wireframe-under-asphalt effect
- **Cats Eyes**: Enhanced with dual-layer glow effect
  - Australian-style dual pairs positioned ON white lines (not between)
  - Outer glow layer (2.5x core size, semi-transparent yellow)
  - Bright core reflector for high visibility
  - Bigger size (0.012 units) for prominence

**New CLI Features:**
- **Snapshot Option**: `--snapshot <seconds>` to auto-save screenshot for visual debugging
  - Saves to `snapshot.png` in current directory
  - Example: `./MusicVisVibeCode.exe --vis cybervalley2 --snapshot 2 --timeout 5`
  - Useful for quickly capturing visual state without manual screenshots

**Build Process:**
- Added `taskkill` command to build workflow to prevent file lock errors
- Ensures clean builds even if previous instance is running

These changes create a more authentic 1980s synthwave/outrun aesthetic with better road definition and a more dramatic celestial body that properly "sets" into the horizon.
