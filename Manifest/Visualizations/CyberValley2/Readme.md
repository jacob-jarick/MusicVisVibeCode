# Visualization: Cyber-Valley

## Description

A classic 80s/Synthwave "Outrun" valley. The visualization features a dual-sided, mirrored heightmap that forms a massive canyon, sloping downward toward a central path that leads to the horizon.

### 1. The Day/Night Cycle (The "Long Vibe")

* **Rotation**: The Sun and Moon are positioned on an invisible wheel centered on the horizon.
* **Timing**: A full cycle (Day to Night and back) takes **10 minutes**.
* **The Celestial Transition**:
* As the Sun sets, the sky colors should shift from vibrant Pink/Orange to deep Indigo/Violet.
* The Moon rises as the Sun sinks below the 40% horizon line.


* **V-Key Override**: The `V` key manually toggles the current state (Sun mode vs. Moon mode), but the 10-minute rotation continues in the background.

### 2. The Valley Geometry (Mirrored "V" Slope)

* **The Horizon**: Fixed at 40% from the top of the screen.
* **The Slope**:
* **Outer Edges (Far Left/Right)**: This is where the **Bass** (low frequencies) lives. These should be the tallest points of the "mountains."
* **Center Path**: This is where the **Treble** (high frequencies) lives. It should be relatively flat, creating a "valley floor" or road effect.
* **Result**: The spectrum creates a natural "V" shape that focuses the user's eye on the center of the horizon.



### 3. Audio Synchronization & "The Kick"

* **The Bass Thump**:
* **Sun/Moon Pulse**: The celestial body currently in the sky should subtly expand and increase its glow/bloom intensity in sync with the **Bass** (bins 0-5).
* **World Shake**: High-intensity bass hits (detected via `HistoryNormalized`) should trigger a slight "Chromatic Aberration" or "Camera Jitter" effect.


* **High-End Glitter**:
* Use the **Treble** (bins 200-255) to trigger "Glitch Stars" or flickering neon highlights on the grid lines of the valley walls.



### 4. Atmosphere & Aesthetics

* **Color Palette**:
* **Day**: Neon Pink (#FF007F) and Sunset Orange (#FF8C00).
* **Night**: Electric Blue (#00FFFF) and Cyber Purple (#8A2BE2).


* **The Grid**: Wireframe lines that flow toward the horizon. The "floor" should feel like it is moving at a constant speed, independent of the audio.

## Controls

* **V**: Toggle between "Sun" (Vaporwave) and "Moon" (Cyberpunk) modes.
* **G**: Toggle Grid visibility (Solid vs. Wireframe).
* **- / =**: Decrease/Increase the speed of the "Flow" (movement toward the horizon).

---

### Integration Note for the Agent

Ensure the `CyberValley` class pulls from `SpectrumHighestSample[256]` for the mountain heights to prevent the "canyon walls" from flickering too violently. Use a simple linear interpolation (Lerp) when the Sun and Moon swap to ensure the sky color transition isn't a hard cut.

**Would you like me to help you set up the "Transition Logic" in the Main Manifest so that switching between the Spectrum vis and this Cyber-Valley vis has a smooth fade-to-black?**