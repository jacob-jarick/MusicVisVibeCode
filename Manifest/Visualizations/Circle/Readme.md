# about

Inspired by one of the milkdrop vis from winamp.

A Circle made from mirrored Spectrum line analyser (see linefader for line example), 

Circle in the center of the screen.

The spectrum is always mirroerd (like mirror option in linefader). To be clear reference mirror mode for line fader. it would look like this ABCCBA

Circle will slowly rotate

# controls

- ,/.: decrease/ increasse fade percentage. Max fade 5% min fade 0% (off) increments of 0.05%, default 1%
- -/=: decrease/ increase zoomout percentage, Max 5%, min 0% (off) increments of 0.05%, default 1%
- ;/': decrease/ increase blur percentage, Max 10%, min 0% (off) increments of 0.05%, default 1%
- k/l: change rotation degrees amount, Min -1.5 degrees, max 1.5 degrees, (0 degrees = stop rotation), increments 0.1 degrees, default 0.1 degrees.
- m: cycle peaks between inside of circle and outside and both 
- z: toggle zoom out / zoom in (default zoom out)
- p: toggle "fill in line" and "line no fill" (default no fill)
 
# Design

Supports Backgound

info screen should show

- peaks on inside / outside
- fade percentage
- zoomout percentage

Use full spectrum (all 256) to draw a smoothed (moving average, no sharp spikes, just do rolling average of 3)

use a Texture Feedback Loop in DirectX for zoomout & fade (or most efficient alternative).

Update will follow this pattern.

- take entire screen (excluding background) 
- fade by fade percentage
- blur by blur percentage
- zoomout by zoomout percentage (take entire scene minus bg, and do a centered zoom out)
- change circle colour slightly (cycle through rainbow spectrum)
- draw new circle (rotation advanced from previous position if rotation is not 0) .

Note: rotate circle not texture.
