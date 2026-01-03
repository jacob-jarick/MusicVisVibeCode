# Visualization: Cyber-Valley (Fixed Logic)

## Core Concept: The "Stamp and Scroll"
The mountains are not a live-reacting mesh. Instead, they are a series of historical "snapshots" of the spectrum. 

### 1. The Mountain Logic (Immutable History)
- **Generation**: At a fixed interval (tied to the 'Speed' variable), take the current `SpectrumNormalized` and "stamp" it as a new line at the bottom of the screen.
- **Immutability**: Once a mountain line is created, its height data MUST NEVER update again. It is a frozen piece of history.
- **Movement**: This line then travels toward the horizon at the exact same speed as the road grid.
- **Perspective**: As the line moves toward the horizon, scale its width and height down to converge at the vanishing point.

### 2. Geometry & Mapping
- **V-Shape**: Map Bass to the far left/right edges and Treble toward the center. 
- **Flat Lines**: Draw the mountains as flat lines (no vertical slanting). The mirrored spectrum naturally creates the "V" canyon look.
- **Depth**: You should maintain a buffer of roughly 30-50 lines to fill the distance between the viewer and the horizon.

### 3. Synchronized Speed
- **Global Velocity**: The variable `m_cv2Speed` must control the retreat of:
    1. The Road Grid (Texture offset/scrolling).
    2. The Starfield (Stars moving toward the viewer).
    3. The Mountain Lines (Translation toward the horizon).
- All three elements must appear to move through space at the same velocity.