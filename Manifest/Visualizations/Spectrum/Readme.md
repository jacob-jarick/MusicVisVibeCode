# Description

Simple 16 bar horizontal spectrum analyzer with decaying peaks.

with 16 segments per bar.

Segments use green,yellow,orange,red colour gradient like classic spectrum analyzers.

Horizontal gap of 1 segment between top of each bar and then one more segement for when peak hits max

default decay, 1 veritcal segment every 0.2 seconds

Segments should be semi transparent, 50% transparency.

# Data curation

Trim the highest 32 frequencies from Spectrum

then use the remainder over the 16 buckets.

Use highest value from each bucket to set each vu bar.

# Controls

- **-**: reduce peak decay speed 
- **=**: increase peak decay speed

# Background

Default: Black

