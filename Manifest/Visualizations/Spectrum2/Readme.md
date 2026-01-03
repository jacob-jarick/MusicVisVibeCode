# Description

28 bar horizontal spectrum analyzer with decaying peaks.

48 Segements High per vu bar.

Segments use green,yellow,orange,red colour gradient like classic spectrum analyzers but fade grandual using a nice gradient.

round edges of vu segements, put a slight darker border on them to.

Horizontal gap of 1 segment between top of each bar and then one more segement for when peak hits max

default decay, 1 veritcal segment every 0.2 seconds

Segments should be semi transparent, 65% transparency (also decaying peaks should be 50% transparent).

# Data curation

Trim the highest 32 frequencies from Spectrum

then use the remainder over the 28 buckets (4 frequency each).

Use highest value from each bucket to set each vu bar.

# Controls

- **-**: reduce peak decay speed 
- **=**: increase peak decay speed

- **m**: rotate through mirroring of spectrum
  - No mirror
  - Mirror with bass at edge of screen. left side = Spectrum, Right = Flipped spectrum. This would be the default
  - Alt Mirror with bass in centre, Left side = flipped spectrum, right side = Spectrum 

# Background

Default: Random Image

