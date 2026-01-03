# about

Inspired by one of the milkdrop vis from winamp 

# controls

- ,/.: decrease/ increasse fade. Max fade 0.5% min fade 0.05% increments of 0.05%
- -/=: decrease/ increase scroll speed in units of pixels. min 1... max.. 50
- m: rotate through mirroring of spectrum
  - No mirror
  - Mirror with bass at edge of screen. left side = Spectrum, Right = Flipped spectrum. This would be the default
  - Alt Mirror with bass in centre, Left side = flipped spectrum, right side = Spectrum 
 
# Design

Supports Backgound

info screen should show speed and if mirrored.

Use full spectrum (all 256) to draw a smoothed (moving average, no sharp spikes, just do rolling average of 3)

Update will follow this pattern.

- take all existing lines, move up screen by set pixel amount
- Fade all lines, with goal being oldest highest will be darkest.
- Draw new line down bottom of screen.
