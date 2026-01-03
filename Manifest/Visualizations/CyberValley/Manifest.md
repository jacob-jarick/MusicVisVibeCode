# Visualization: Cyber-Valley Manifest

## Description
A classic 80s/Synthwave "Outrun" valley visualization.
Features a dual-sided, mirrored heightmap forming a canyon, sloping to a central horizon.

## Visual Elements

### 1. The Valley (Terrain)
- **Geometry**: Mirrored heightmap.
  - **Outer Edges**: High frequencies (Bass/Low End) - Tallest mountains.
  - **Center**: Low frequencies (Treble/High End) - Flat valley floor/road.
  - **Mapping**: `SpectrumHighestSample` mapped from index 0 (edges) to 255 (center)?? 
    - *Correction based on Readme*: "Outer Edges... Bass... Center Path... Treble".
    - Standard FFT: Index 0 is Bass, Index 255 is Treble.
    - So, Map Index 0 to Outer Edges, Index 255 to Center.
- **Horizon**: Fixed at 40% from top of screen (Y = 0.2 in NDC if Top is 1.0, Bottom is -1.0? Need to map 0-1 screen space).
- **Grid**: Wireframe lines flowing toward horizon.

### 2. Day/Night Cycle
- **Cycle Duration**: 10 minutes full loop.
- **Celestial Bodies**: Sun and Moon on a rotational wheel.
- **Transition**:
  - **Day**: Neon Pink (#FF007F) / Sunset Orange (#FF8C00).
  - **Night**: Electric Blue (#00FFFF) / Cyber Purple (#8A2BE2).
  - Smooth Lerp between palettes.

### 3. Audio Reactivity
- **Bass (Bins 0-5)**:
  - Pulse the Sun/Moon (Scale/Bloom).
  - "World Shake" / Chromatic Aberration on high intensity.
- **Treble (Bins 200-255)**:
  - "Glitch Stars" or flickering neon on grid lines.

## Controls
- **V**: Toggle Sun/Moon mode (Manual override of cycle).
- **G**: Toggle Grid (Solid vs Wireframe).
- **- / =**: Adjust Flow Speed.

## Implementation Details
- **Class/Method**: `RenderCyberValley()`
- **Data Source**: `SpectrumHighestSample` (to reduce flicker).
