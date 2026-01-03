# Spectrum2 Visualization Manifest

## Overview
A high-resolution 28-bar horizontal spectrum analyzer with smooth gradients, rounded segments with borders, and decaying peaks. This is an enhanced version of the classic spectrum visualization with more bars and segments for greater detail.

## Visual Design

### Layout
- **Bars**: 28 vertical bars
- **Segments per Bar**: 48 segments (high resolution)
- **Bar Spacing**: 1 segment horizontal gap between bars
- **Peak Gap**: 1 segment gap between bar top and peak indicator, plus 1 additional segment for max peak

### Segment Styling
- **Colors**: Smooth gradient from green (low) → yellow → orange → red (high)
  - Green: Segments 0-12
  - Yellow: Segments 13-24
  - Orange: Segments 25-36
  - Red: Segments 37-48
- **Transparency**: 65% transparent (alpha = 0.35)
- **Shape**: Rounded edges for polished appearance
- **Border**: Slight darker border around each segment for definition

### Peak Indicators
- **Color**: Red with smooth gradient
- **Transparency**: 50% transparent (alpha = 0.5)
- **Decay Rate**: Default 1 vertical segment every 0.2 seconds (5 segments/second)
- **Behavior**: Persist at maximum reached level and gradually decay

### Background Support
- **Default**: Random image from Backgrounds folder
- **Behavior**: Always loads a random background on startup if available

## Data Curation

### Frequency Range
- Trims the highest 32 frequency bins (224-255) which contain less active high-end data
- Uses bins 0-223 (224 bins total)

### Bucket Distribution
- Divides 224 bins into 28 buckets
- 8 bins per bucket (224 ÷ 28 = 8)
- Each bar displays the maximum value from its corresponding bucket

## Controls (Visualization-Specific)

### Decay Speed
- `-` (Minus): Decrease peak decay speed
- `=` (Equals): Increase peak decay speed

### Mirror Modes
- `M`: Cycle through mirroring modes:
  1. **No Mirror**: Single full-width spectrum
  2. **Bass at Edges** (Default): Mirror at center, bass frequencies at screen edges
     - Left half: Normal spectrum
     - Right half: Flipped spectrum (mirrored)
  3. **Bass in Center**: Mirror at edges, bass frequencies at screen center
     - Left half: Flipped spectrum
     - Right half: Normal spectrum

## Info Display Requirements

When Info Overlay is shown, display:
- Current decay rate (segments per second)
- Current mirroring mode:
  - "No Mirror"
  - "Mirror: Bass at Edges"
  - "Mirror: Bass in Center"

## Technical Implementation Notes

### Rendering Strategy
- Use DirectX 11 for efficient rendering
- Render segments as rounded rectangles with borders
- Apply color gradients smoothly across segment ranges
- Use alpha blending for transparency effects

### Data Processing
- Use `SpectrumNormalized[256]` for input data
- Apply bucketing: 224 bins → 28 buckets → 8 bins per bucket
- Scale normalized values to 48 segments (multiply by 48)
- Track peak values per bar with time-based decay

### Performance Considerations
- Batch all segment quads for single draw call
- Reuse vertex buffer with D3D11_MAP_WRITE_DISCARD
- Minimize state changes during rendering

## Integration
- Must be registered in visualization list as index 3
- Must respond to all global controls
- Must work seamlessly with background system
- Help text must display both global and vis-specific controls
