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
- **Position**: STATIC - Centered on horizon (X=0), positioned so bottom 1/5th is below horizon.
- **Size**: Radius 0.30 in NDC (twice original size).
- **Aspect Ratio Correction**: X coordinates scaled by 1/aspectRatio for perfect circle on all screens.
- **Day (Sun)**: Solid Yellow (#FFFF00) with glow halo.
  - Vaporwave horizontal stripes: 5 transparent gaps (sky-colored cut-outs) across lower 40%, creating classic retro aesthetic.
- **Night (Moon)**: Pale White/Blue (#CCCCFF) with glow halo.
  - Vaporwave horizontal stripes: 5 transparent gaps (sky-colored cut-outs) across lower 40%, creating classic retro aesthetic.
- **Toggle**: V key switches between Day/Night modes (does NOT rotate).
- **Audio Reactivity**: NONE. Celestial bodies are completely static.
- **Rendering**: Stripes use sky gradient color to create "see-through" effect, showing the background sky.

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

### 4. Road Rendering

#### Surface
- **Color**: Dark asphalt blue-gray (#141418 / RGB 0.08, 0.08, 0.10) for bitumen appearance.
- **Drawn from**: Horizon to bottom of screen.

#### Road Edge Lines
- **Position**: Two white lines at the outer edges of the road (left and right boundaries).
- **Style**: Bright white (#FFFFFF) continuous lines, slightly thicker (0.004 units).
- **Segments**: 30 vertical segments scrolling with `m_gridOffset`.
- **Perspective**: Same as grid (perspScale = 1.0f - z * 0.7f).
- **Purpose**: Define road boundaries and enhance perspective depth.

#### Dual White Center Lines
- **Position**: Two parallel white lines running down the exact center of the road.
- **Spacing**: 0.01 NDC units apart (before perspective scaling).
- **Style**: Bright white (#FFFFFF) for high visibility.
- **Segments**: 30 vertical segments scrolling with `m_gridOffset`.
- **Perspective**: Same as grid (perspScale = 1.0f - z * 0.7f).
- **Rendering**: Lines converge toward vanishing point at horizon center.

#### Cats Eyes (Reflectors)
- **Position**: Australian-style dual pairs ON each white line (not between).
- **Frequency**: Every 3rd road segment (spaced for realism).
- **Design**: Two-layer glow effect:
  - Outer glow: Large semi-transparent yellow halo (2.5x core size, 40% opacity).
  - Core reflector: Bright yellowish-white diamond (#FFFFE6 / RGB 1.0, 1.0, 0.8).
- **Size**: 0.012 NDC units (scaled with perspective) - highly visible.
- **Pairs**: One cat's eye on left white line, one on right white line.
- **Purpose**: Add depth cues and classic highway aesthetic with realistic reflector glow.

#### Grid Lines (Optional Overlay)
- **Style**: Thin wireframe lines with soft overlay effect.
- **Color**: Softened mountain color at 30% opacity (subtle wireframe visible under asphalt).
- **Toggle**: `G` key toggles grid visibility.
- **Width**: 0.15 half-width (X from -0.15 to 0.15, scaled with perspective).
- **Vertical Lines**: 5 evenly spaced lines converging from bottom to horizon center.
- **Horizontal Lines**: 15 lines, using same `m_gridOffset` for synchronized scrolling.
- **Purpose**: Subtle depth grid like wireframe visible through road texture.

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
3. **Synchronization**: `m_gridOffset` drives mountains, road, dual lines, cats eyes, and starfield movement together.
4. **Aspect Ratio**: Sun/Moon X coordinates are scaled by 1/aspectRatio to maintain perfect circle on all screen sizes.
5. Draw sky gradient as a full-screen quad from top to horizon.
6. Draw ground as a solid asphalt-colored quad from horizon to bottom.
7. Mountains are drawn as connected line segments (thick lines using perpendicular offset).
8. Dual white center lines and cats eyes render in front of grid (lower Z values).
9. All geometry uses solid color mode (UV = {-1, -1} to skip texture sampling).
10. **Bins Used**: First 224 bins only (bins 0-223), excluding high-frequency bins 224-255.

## Controls

| Key | Action |
| --- | ------ |
| V | Toggle Day/Night Mode (Sun/Moon) |
| G | Toggle Road Grid Visibility |
| - | Decrease movement speed |
| = | Increase movement speed |

## Member Variables Required
```cpp
// CyberValley2 State
float m_time = 0.0f;              // Timer for animated effects (clouds, stars)
float m_speed = 50.0f;            // Movement speed (5-200%, percentage of 1 unit/sec)
float m_gridOffset = 0.0f;        // Scroll position (0-1, wraps)
bool m_sunMode = false;           // true = Day, false = Night
bool m_showGrid = true;           // Grid visibility toggle
float m_mountainHistory[60][256]; // Frozen historical spectrum snapshots
int m_historyWriteIndex = 0;     // Current write position in circular buffer
float m_timeSinceLastLine = 0.0f; // Time accumulator for 30Hz line generation
```
