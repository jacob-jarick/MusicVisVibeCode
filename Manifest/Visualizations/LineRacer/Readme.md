# Visualization: LineRacer

## Description

A 2D side-scrolling physics toy inspired by "Line Rider" and "Hill Climb Racing." The terrain is a literal, rolling manifestation of the song's bass history. A small, physics-controlled buggy attempts to navigate this "Bass-scape," catching air on drops and struggling on climbs.

### 1. The Terrain (The "Bass-scape")

* **Generation**: The terrain is a single continuous line generated from the **lowest frequency bin** (Bass) of `SpectrumNormalized[0]`.
* **Movement**:
* The terrain "stamps" the current bass value on the far right edge of the screen.
* It scrolls to the left at a constant speed (controlled by `m_cv2Speed` or equivalent).


* **Immutability**: Once a point on the line is created, its height must never change. It is a historical record of the bass moving through space.
* **Visuals**: A thick, soft pastel stroke with a solid color fill below it to create the look of rolling hills.

### 2. The Vehicle (The "Buggy")

* **Aesthetic**: A simple, 2D lo-fi buggy (box body with two circular wheels).
* **Physics Logic**:
* **Gravity**: The vehicle is constantly affected by gravity.
* **Collision**: The terrain line acts as a solid boundary. Wheels should "rest" on the line.
* **Air-Time**: If the terrain drops faster than gravity pulls the car, the car catches "air."


* **AI Driver Behavior**:
* **Overshooting**: On downward slopes, the car gains speed and drifts toward the **right** side of the screen.
* **Lagging**: On steep upward climbs, the car loses momentum and drifts toward the **left** side of the screen.
* **Spring Force**: A subtle "rubber-band" force should always try to pull the car back toward the horizontal center.



### 3. Safety & Respawning

* **Left-Side Despawn**: If the terrain is too steep or the speed is too high and the car is pushed completely against the **left edge** of the screen, it must **despawn**.
* **Sky Respawn**: Upon despawning, the car immediately **respawns** at the horizontal center of the screen, dropping from the very top of the sky to land back on the terrain.

### 4. Aesthetics (Soft Pastel / Kawaii)

* **Theme**: High-brightness, low-saturation pastel colors.
* **Background**: Soft Cream or Mint Green.
* **Terrain**: Pale Peach or Lavender.
* **Vehicle**: Sky Blue or Soft Pink.

## Controls

* **- / =**: Decrease/Increase the terrain scroll speed.
* **R**: Manually trigger a "Sky Respawn" (drops car from the center top).
* **S**: Toggle "Stunt Mode" (multiplies gravity and bounce for more chaotic air-time).

---

### Implementation Notes for the Agent

* Use a `std::deque<float>` to store the terrain heights to ensure "Snap-and-Forget" immutability as the line scrolls.
* For the car physics, calculate the slope between the two points on the terrain directly beneath the wheels to determine the car's rotation (angle).
* Use `deltaTime` for all physics and scrolling to ensure consistent behavior across different frame rates.