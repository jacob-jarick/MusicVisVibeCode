# LineFader Visualization Manifest

## Overview
A scrolling spectrum visualization inspired by Milkdrop visualizations from Winamp. Lines of spectrum data scroll up the screen, fading as they age, creating a trailing history effect.

## Visual Design

### Data Source
- Uses full spectrum: all 256 frequency bins
- Applies smoothing via rolling average of 3 samples to avoid sharp spikes

### Rendering Behavior

**Per Frame Update:**
1. **Scroll**: Move all existing lines upward by a configurable pixel amount (scroll speed)
2. **Fade**: Gradually fade all existing lines, with oldest lines (highest on screen) being darkest
3. **Draw New Line**: Draw a fresh spectrum line at the bottom of the screen

### Spectrum Mirroring Modes
Three mirroring modes, cycled via the 'M' key:

1. **No Mirror**: Single spectrum drawn across full width
2. **Mirror - Bass at Edges** (Default): 
   - Left half: Normal spectrum (bass on left edge)
   - Right half: Flipped spectrum (bass on right edge)
   - Creates symmetry with bass at screen edges
3. **Alt Mirror - Bass in Center**:
   - Left half: Flipped spectrum (bass in center)
   - Right half: Normal spectrum (bass in center)
   - Creates symmetry with bass in screen center

### Background Support
Yes - visualization supports background images as per global specification.

## Controls (Visualization-Specific)

- `,` (Comma): Decrease fade rate by 0.05%
- `.` (Period): Increase fade rate by 0.05%
- `-` (Minus): Decrease scroll speed by 1 pixel
- `=` (Equals): Increase scroll speed by 1 pixel
- `M`: Cycle through mirroring modes (No Mirror → Bass at Edges → Bass in Center → repeat)

**Fade Rate Range**:
- Minimum: 0.05% per frame (slowest fade, longest trails)
- Maximum: 0.50% per frame (fastest fade, shortest trails)
- Default: 0.50% per frame
- Increment: 0.05%

**Scroll Speed Range**: 
- Minimum: 1 pixel per frame
- Maximum: 50 pixels per frame

## Info Display Requirements

When Info Overlay is shown, display:
- Current scroll speed (in pixels)
- Current fade rate (as percentage)
- Current mirroring mode:
  - "No Mirror"
  - "Mirror: Bass at Edges"
  - "Mirror: Bass in Center"

## Technical Implementation Notes

### Smoothing Algorithm
Apply a rolling average of 3 samples:
```
smoothed[i] = (spectrum[i-1] + spectrum[i] + spectrum[i+1]) / 3
```
Handle edge cases (i=0 and i=255) appropriately.

### Line Scrolling
- Maintain a texture or buffer of previous lines
- Each frame, shift content upward by scroll speed
- Apply fading to existing content
- Draw new line at bottom

### Fading Strategy
- Linear or exponential fade based on line age
- Oldest lines should be nearly invisible
- Suggest fade rate that makes lines disappear before reaching top of screen at typical scroll speeds

### Performance Considerations
- Reuse buffers/textures where possible
- Consider using shader for efficient scrolling and fading operations
- Render smoothed spectrum data for visual appeal

## Data Usage
- Primary data source: `SpectrumNormalized[256]` or `SpectrumHighestSample[256]`
- Respect global 'N' key toggle for normalized vs raw values
- Use `playing` flag to pause/resume visualization updates

## Integration
- Must be registered in visualization list
- Must respond to all global controls
- Must display proper help text when 'H' is pressed
- Must work seamlessly with background system
