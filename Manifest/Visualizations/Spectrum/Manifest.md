# Visualization Manifest: Spectrum

## Description
Simple 16-bar horizontal spectrum analyzer with decaying peaks.

## Specifications
- **Layout**: 16 vertical bars.
- **Segments**: 16 segments per bar.
- **Transparency**: Segments should be semi-transparent (50% alpha).
- **Colors**: Gradient from Green (low) -> Yellow -> Orange -> Red (high).
- **Spacing**: 
  - Horizontal gap of 1 segment between bars (implied, or just visual separation).
  - 1 segment gap between top of bar and peak indicator.
- **Peak Behavior**:
  - Peaks persist and decay over time.
  - Default decay rate: 5 vertical segments per second (1 segment every 0.2s).
  - **Color**: Red.
- **Background**: Black (or Image if loaded).

## Controls
- `-`: Decrease peak decay speed.
- `=`: Increase peak decay speed.
- `B`: Toggle Background Image (loads random from Backgrounds/).
