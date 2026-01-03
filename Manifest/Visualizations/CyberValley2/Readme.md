# Visualization: Cyber-Valley

## Description

A perspective-based 3D "Infinite Runner" visualization. The scene consists of a central road/grid flanked by mountains that are generated in real-time by the audio spectrum. All elements originate at the bottom of the screen and retreat towards a fixed horizon.

### 1. The Celestial Cycle (Sun & Moon)

* **Motion**: The Sun and Moon follow a continuous circular rotation centered on the horizon line.
* **Timing**: A full "Day/Night" cycle (360-degree rotation) takes exactly **10 minutes**.
* **Behavior**: These are static, glowing orbs. **Do not apply audio-reactive shaking or scaling** to the celestial bodies themselves.


### 2. The Valley Geometry (Mountains & Road)

* **Perspective**: A fixed horizon line is set at 40% from the top of the screen.
* **The Mountains**:
* Generated using the **latest spectrum line**.
* **Mapping**: Bass frequencies are mapped to the **outer edges** (far left and far right). Higher frequencies slope down toward the center.
* **Effect**: This creates a mirrored "V" shape, forming a valley/canyon that frames the center of the screen.

note:

```
Ive noted the mountain lines are slanted in a V shape, diagonal ? but this wont bee needed due to the typical shape of the spectrum, when mirrored it will look like a rough V so draw them Flat.
```

* **Additional Ambience**

Day (Sun): Add some vapourwave clouds, that move gently

Night (Moon): Should have twinkling stars, add the odd shooting star randomly. 
Make stars move towards user like a simple starfield effect, ensure only above horizon, this will include the shoot stars

* **The Movement**:
* The "Latest" spectrum line is drawn at the bottom of the screen.
* On every update, existing lines are scaled down and translated toward the horizon to simulate depth.


* **The Center**: A grid-patterned road sits in the center between the mountains, also moving away from the viewer toward the horizon.

### 3. Audio & Aesthetics

* **Colors**:
* **Day**: Neon Pink/Orange gradients.
* **Night**: Deep Blues/Purples.


* **Sync**: While the mountains are formed by the spectrum, the overall "Flow" speed is constant unless modified by user controls.

## Controls

* **V**: Toggle between Sun (Vaporwave) and Moon (Cyberpunk) sky states.
* **G**: Toggle Grid visibility on the road.
* **- / =**: Decrease/Increase the speed of the horizon retreat.

