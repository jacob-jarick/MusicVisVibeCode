# Visualization Manifest: CyberValley2

## Description
A perspective-based 3D "Infinite Runner" visualization featuring a retro synthwave canyon. Mountains are generated in real-time from the audio spectrum, flanking a central grid road that retreats toward a fixed horizon. Features a continuous Sun/Moon day-night cycle.

## Core Specifications

### 1. Coordinate System & Perspective
- **Horizon Line**: Fixed at Y = 0.2 in NDC (40% from top of screen).
- **Vanishing Point**: Center of screen at horizon (X=0, Y=0.2).
- **Drawing Order**: Sky -> Sun/Moon -> Mountains -> Road Grid (back to front for proper layering).

### 2. Sky & Celestial Bodies

#### Sky Gradient
- **Day Mode**: 
  - Top: Sunset Orange (#FF8C00 / RGB 1.0, 0.55, 0.0)
  - Bottom (at horizon): Neon Pink (#FF0080 / RGB 1.0, 0.0, 0.5)
- **Night Mode**:
  - Top: Dark Indigo (#000033 / RGB 0.0, 0.0, 0.2)
  - Bottom (at horizon): Cyber Purple (#8000CC / RGB 0.5, 0.0, 0.8)

#### Sun/Moon
- **Position**: Centered horizontally at the horizon (X=0, Y=0.5 in NDC, above horizon).
- **Size**: Radius 0.15 in NDC.
- **Day (Sun)**: Solid Yellow (#FFFF00).
- **Night (Moon)**: Pale White/Blue (#CCCCFF).
- **Rotation**: Full 360-degree cycle over 10 minutes (600 seconds). The orb position follows `sin/cos` of cycle angle.
- **Audio Reactivity**: NONE. Celestial bodies are static orbs.

### 3. Mountain Geometry (Audio-Reactive Valley)

#### Spectrum Mapping (Mirrored "V" Shape)
- **Left Side (X < 0)**: Spectrum bins 0-127 mapped from edge (X=-1) to center (X=0).
  - Bin 0 (Bass) at far left edge (highest peak).
  - Bin 127 at center (lowest, valley floor).
- **Right Side (X > 0)**: Mirrored. Spectrum bins 0-127 mapped from center (X=0) to edge (X=1).
  - Bin 0 (Bass) at far right edge (highest peak).
  - Bin 127 at center (lowest, valley floor).
- **Height**: `height = spectrum_value * max_height_scale` (e.g., max_height_scale = 0.6).

#### Line Generation & Movement
- **New Line**: Each frame, generate a new spectrum line at the **bottom** of the screen (Y = -1.0).
- **History**: Store up to 60 lines (matching audio History buffer).
- **Movement**: Each line moves **toward the horizon** at a constant speed (`m_cv2Speed`).
- **Perspective Scaling**: As lines approach horizon:
  - Y position interpolates from -1.0 to 0.2 (horizon).
  - X coordinates scale down (converge to vanishing point).
  - Height scales down proportionally.

#### Rendering
- **Style**: Wireframe lines connecting adjacent points.
- **Color (Day)**: Magenta/Pink (#FF00CC).
- **Color (Night)**: Cyan (#00FFFF).

### 4. Road Grid (Center Path)

#### Geometry
- **Width**: Center 20% of screen (X from -0.1 to 0.1, scaled with perspective).
- **Vertical Lines**: 5 evenly spaced lines from bottom to horizon.
- **Horizontal Lines**: 20 lines, scrolling toward horizon at same speed as mountains.

#### Rendering
- **Style**: Thin wireframe lines.
- **Color**: Same as mountain grid color (Magenta Day / Cyan Night).
- **Toggle**: `G` key toggles grid visibility.

### 5. Movement & Speed

- **Base Speed**: 0.5 units per second (tunable).
- **Controls**: `-` decreases speed, `=` increases speed.
- **Range**: 0.1 to 2.0 units per second.

## Controls Summary
| Key | Action |
|-----|--------|
| V | Toggle Day/Night (Sun/Moon) mode |
| G | Toggle road grid visibility |
| - | Decrease movement speed |
| = | Increase movement speed |

## Member Variables Required
```cpp
// CyberValley2 State
float m_cv2Time = 0.0f;           // Day/night cycle timer (0-600 seconds)
float m_cv2Speed = 0.5f;          // Movement speed
float m_cv2GridOffset = 0.0f;     // Grid scroll position (0-1)
bool m_cv2SunMode = true;         // true = Day, false = Night
bool m_cv2ShowGrid = true;        // Grid visibility toggle
```

## Implementation Notes
1. Use `SpectrumNormalized[256]` for mountain height values.
2. Draw sky gradient as a full-screen quad from top to horizon.
3. Draw ground as a solid color quad from horizon to bottom.
4. Mountains are drawn as connected line segments (triangle strips for thickness).
5. All geometry uses solid color mode (UV = {-1, -1} to skip texture sampling).
