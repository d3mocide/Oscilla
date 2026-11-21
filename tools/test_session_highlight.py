#!/usr/bin/env python3
"""Pixel regression on the actual C++ session catalog source render."""
from pathlib import Path
import sys
path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('/tmp/oscilla-viewport-review/sessions-catalog-active.ppm')
header, pixels = path.read_bytes().split(b'\n255\n', 1)
assert header.splitlines() == [b'P6', b'320 240']
def pixel(x, y):
    offset = (y * 320 + x) * 3
    return pixels[offset:offset + 3]
ink = pixel(160, 148)  # Void Ink inside the upper aperture
# Row 2 (0-indexed) is selected in the active fixture: y=94..115, x=14..305.
background = pixel(296, 105)
assert background != ink, 'Selected fixture must have a filled highlight'
assert all(pixel(x, y) != ink for y in range(94, 116) for x in range(14, 306)), 'Text punches black holes through the selected row'
assert pixel(25, 44) == ink, 'Unselected row must retain the normal Void Ink background'
print('session highlight: full selected fill under both labels, normal unselected background passed')
