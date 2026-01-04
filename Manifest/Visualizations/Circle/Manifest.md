# Circle Visualization - Technical Specification

## Overview
A circular spectrum visualization inspired by Milkdrop from Winamp. Features a rotating circle made from mirrored spectrum data with advanced feedback effects including fade, blur, and zoom-out.

## Visual Design

### Circle Geometry
- **Position**: Centered on screen
- **Spectrum Data**: Use full 256 frequency bins
- **Mirroring**: Always mirrored (ABCCBA pattern) - similar to LineFader mirror mode
- **Smoothing**: Apply 3-sample rolling average to prevent sharp spikes
- **Rotation**: Continuous rotation at configurable speed

### Rendering Pipeline
1. Apply fade to entire screen (excluding background) by fade percentage
2. Apply blur effect by blur percentage
3. Apply centered zoom-out by zoomout percentage
4. Cycle circle color through rainbow spectrum
5. Draw new circle at current rotation angle

### Effects Implementation
- Use DirectX Texture Feedback Loop (or most efficient alternative)
- Rotate the circle geometry, not the texture
- Background rendering independent from effects chain

## Audio Data
- **Primary Source**: SpectrumNormalized[256] or SpectrumHighestSample[256]
- **Smoothing**: 3-sample rolling average
- **Peak Mode**: Toggle between inside and outside of circle

## Controls

### Main Controls
Inherits all main controls from parent application

### Visualization-Specific Controls
| Key | Action | Range | Increment | Default |
|-----|--------|-------|-----------|---------|
| `,` / `.` | Decrease/Increase fade % | 0-5% | 0.05% | 1% |
| `-` / `=` | Decrease/Increase zoom-out % | 0-5% | 0.05% | 1% |
| `;` / `'` | Decrease/Increase blur % | 0-10% | 0.05% | 1% |
| `k` / `l` | Decrease/Increase rotation speed | -1.5° to 1.5° | 0.1° | 0.1° |
| `m` | Toggle peaks inside/outside circle | N/A | N/A | Inside |

Note: 0 degrees = no rotation

## Info Screen Display
When info screen is active (key `i`), display:
- Peaks mode: "Inside" or "Outside"
- Fade percentage: "Fade: X.XX%"
- Zoom-out percentage: "Zoom: X.XX%"
- Blur percentage: "Blur: X.XX%"
- Rotation speed: "Rotation: X.X°"

## Configuration Persistence
All visualization-specific settings should be saved to and loaded from user config file:
- fade_percentage (float, 0.0-5.0)
- zoomout_percentage (float, 0.0-5.0)
- blur_percentage (float, 0.0-10.0)
- rotation_speed (float, -1.5 to 1.5)
- peaks_inside (bool)

## Technical Requirements

### DirectX Implementation
- Support background rendering
- Implement efficient texture feedback loop
- Handle rotation via geometry transformation
- Smooth color transitions through HSV color space

### Performance Considerations
- Optimize blur and zoom operations
- Minimize texture copies
- Efficient spectrum data smoothing

### Integration
- Register in visualization factory
- Add to CLI --vis option as "circle"
- Include in disable menu system
- Follow existing visualization interface pattern

## Code Organization
- Header: `src/visualizations/Circle.h`
- Implementation: `src/visualizations/Circle.cpp`
- Inherits from base Visualization class
- Implements all required virtual methods
