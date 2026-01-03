# Visualization Manifest: CyberValley2

## Description
A perspective-based 3D "Infinite Runner" visualization featuring a retro synthwave canyon. Mountains are **immutable historical snapshots** of the audio spectrum that scroll toward the horizon, flanking a central grid road. Features a static Sun/Moon above the horizon with day/night toggle.

## Core Concept: "Stamp and Scroll"
Mountains are NOT live-reacting meshes. They are frozen historical snapshots:
- **Generation**: New spectrum lines are "stamped" at the bottom of the screen
- **Immutability**: Once created, a mountain line's height data NEVER updates again
- **Movement**: Lines travel toward the horizon at the same speed as the road grid
- **Perspective**: Lines scale down in width and height as they approach the vanishing point

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
- **Position**: STATIC - Centered above the horizon (X=0, Y=0.55 in NDC).
- **Size**: Radius 0.15 in NDC.
- **Day (Sun)**: Solid Yellow (#FFFF00) with glow halo.
- **Night (Moon)**: Pale White/Blue (#CCCCFF) with glow halo.
- **Toggle**: V key switches between Day/Night modes (does NOT rotate).
- **Audio Reactivity**: NONE. Celestial bodies are completely static.

### 3. Mountain Geometry (Immutable Historical Snapshots)

#### Spectrum Mapping (Mirrored "V" Shape)
- **Total Bins Used**: 224 bins (256 minus top 32 high-frequency bins)
- **Left Mountain**: Bins 0-111 mapped from left edge to center.
  - Bin 0 (Bass) at far left edge (tall peak).
  - Bin 111 (Mid-high) at center road edge (lower).
- **Right Mountain**: Bins 111-0 (mirrored) mapped from center to right edge.
  - Bin 111 (Mid-high) at center road edge (lower).
  - Bin 0 (Bass) at far right edge (tall peak).
- **Result**: Natural "V" valley shape with bass peaks at outer edges.
- **Resolution**: 112 points per mountain side for smooth rendering.
- **Height**: `height = spectrum_value * MAX_HEIGHT` (MAX_HEIGHT = 1.2).

#### Critical Implementation: Frozen Snapshots
- **History-Based**: Each visible line uses `HistoryNormalized` data based on its scroll position.
- **Z to History Mapping**: `histOffset = (int)(z * 60.0f)` where z is the line's depth (0=bottom, 1=horizon).
- **Freeze Logic**: A line at z=0.5 (halfway to horizon) always shows data from 30 frames ago.
- **No Live Updates**: Lines DO NOT react to current audio; they display historical data only.
- **Scrolling**: As `m_cv2GridOffset` increases, all lines move toward horizon (z increases).

#### Movement & Perspective
- **Speed**: Controlled by `m_cv2Speed` (0.01 to 0.2 lines/sec).
- **Position**: Lines draw at 30 fixed row positions, scrolling via offset.
- **Perspective Scaling**: `perspectiveScale = 1.0f - z * 0.9f` (1.0 at bottom, 0.1 at horizon).
- **Y Position**: `baseY = -1.0f + z * (HORIZON_Y + 1.0f)` (interpolates bottom to horizon).

#### Rendering
- **Style**: 0.15 half-width (X from -0.15 to 0.15, scaled with perspective).
- **Vertical Lines**: 5 evenly spaced lines converging from bottom to horizon center.
- **Horizontal Lines**: 30 lines, using same `m_cv2GridOffset` as mountains for synchronized scrolling.

#### Rendering
- **Style**: Thin wireframe lines.
- **Color**: Same as mountain color (Magenta Day / Cyan Night).
- **Toggle**: `G` key toggles grid visibility.

### 5. Atmospheric Effects

#### Day Mode (Sun)
- **Vaporwave Clouds**: 5 semi-transparent pink/white clouds drifting horizontally above horizon.
- **Movement**: Slow horizontal drift independent of main speed.

#### Night Mode (Moon)
- **Starfield**: 80 twinkling stars moving toward viewer (z-depth scrolling with `m_cv2GridOffset`).
- **Distribution**: Only above horizon, with perspective (closer = larger/brighter).
- **Shooting Stars**: Random shooting stars every 3 seconds, also moving toward viewer in z-space.

### 6. Movement & Speed

- **Default Speed**: 0.05 lines per second.
- **Controls**: `-` decreases by 0.01, `=` increases by 0.01.
- **Range**: 0.01 to 0.2 lines per second.
- **Synchronized**: Road grid, mountains, and starfield all use `m_cv2Speed` and `m_cv2GridOffset` (Magenta Day / Cyan Night).
- **Toggle**: `G` key toggles grid viTimer for animated effects (clouds, stars)
float m_cv2Speed = 0.05f;         // Movement speed (0.01 to 0.2 lines/sec)
float m_cv2GridOffset = 0.0f;     // Scroll position (0-1, wraps)
bool m_cv2SunMode = true;         // true = Day, false = Night
bool m_cv2ShowGrid = true;        // Grid visibility toggle
```

## Implementation Notes
1. **Critical**: Use `HistoryNormalized[histIdx][bin]` where `histIdx` is calculated from z-position, NOT from row index.
2. **Mountains**: Each line must reference historical data based on its scrolled position to maintain immutability.
3. **Synchronization**: `m_cv2GridOffset` drives mountains, road, and starfield movement together.
4. Draw sky gradient as a full-screen quad from top to horizon.
5. Draw ground as a solid color quad from horizon to bottom.
6. Mountains are drawn as connected line segments (thick lines using perpendicular offset).
7. All geometry uses solid color mode (UV = {-1, -1} to skip texture sampling).
8. **Bins Used**: First 224 bins only (bins 0-223), excluding high-frequency bins 224-255
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
