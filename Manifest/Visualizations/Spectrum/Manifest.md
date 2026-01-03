# Visualization Manifest: Spectrum

## Description
Simple 16-bar horizontal spectrum analyzer with decaying peaks.

## Specifications
- **Layout**: 16 vertical bars.
- **Segments**: 16 segments per bar.
- **Transparency**: Segments should be semi-transparent (50% alpha). Peak indicators also use 50% transparency.
- **Colors**: Gradient from Green (low) -> Yellow -> Orange -> Red (high).
- **Spacing**: 
  - Horizontal gap of 1 segment between bars (implied, or just visual separation).
  - 1 segment gap between top of bar and peak indicator.
- **Peak Behavior**:
  - Peaks persist and decay over time.
  - Default decay rate: 5 vertical segments per second (1 segment every 0.2s).
  - **Color**: Red with 50% transparency.
  - **Transparency**: 50% alpha (semi-transparent).
- **Background**: Black (or Image if loaded).

## Data Curation
- Trims the highest 32 frequency bins from the spectrum (bins 224-255).
- Uses the remaining 224 bins (0-223) divided into 16 buckets (14 bins per bucket).
- Each bar uses the highest value from its corresponding bucket.

## Controls
- `-`: Decrease peak decay speed.
- `=`: Increase peak decay speed.
- `B`: Toggle Background Image (loads random from Backgrounds/).
