#!/usr/bin/env python3
"""Draw the music note icons the main menu shows in place of the lyrics while there's no singing, as distance fields for the UI's
outlined image drawing, needs numpy, scipy and PIL.

usage: src/scripts/makenoteicons.py data/textures/icons/note.png data/textures/icons/notes.png
"""
import math, sys
from PIL import Image, ImageDraw
K = 4  # drawn this many times bigger, then scaled down for smooth edges
W = 512*K

def head(d, cx, cy, rx=88, ry=62, tilt=-22):
    """A filled oval note head, tilted like a printed note's."""
    pts = []
    a = math.radians(tilt)
    for i in range(72):
        t = 2*math.pi*i/72
        x, y = rx*math.cos(t), ry*math.sin(t)
        pts.append(((cx + x*math.cos(a) - y*math.sin(a))*K, (cy + x*math.sin(a) + y*math.cos(a))*K))
    d.polygon(pts, fill=255)

def stem(d, x, top, bottom, w=30):
    d.rectangle([(x - w)*K, top*K, x*K, bottom*K], fill=255)

def quad(p0, p1, p2, n=40):
    return [((1-t)**2*p0[0] + 2*(1-t)*t*p1[0] + t*t*p2[0], (1-t)**2*p0[1] + 2*(1-t)*t*p1[1] + t*t*p2[1]) for t in (i/n for i in range(n + 1))]

SPREAD = 40  # pixels at 512 over which the distance runs from the edge to either end, as the outlined fonts' images do

def save(mask, name):
    """Saved as a distance field, 0.5 on the edge rising inside and falling outside, for the UI's outlined image drawing."""
    import numpy as np
    from scipy.ndimage import distance_transform_edt
    m = np.array(mask) > 127
    d = (distance_transform_edt(m) - distance_transform_edt(~m))/K
    v = np.clip(0.5 + d/(2*SPREAD), 0, 1)
    img = Image.fromarray((v*255).astype(np.uint8), 'L')
    img.resize((512, 512), Image.BOX).save(name)

# a single quaver: head, stem and a flag curling down from the top of the stem
img = Image.new('L', (W, W), 0); d = ImageDraw.Draw(img)
head(d, 200, 392)
stem(d, 282, 70, 380)
outer = quad((282, 70), (300, 190), (400, 250)) + quad((400, 250), (450, 300), (420, 390))
inner = quad((418, 372), (430, 310), (380, 280)) + quad((380, 280), (282, 230), (282, 160))
d.polygon([(x*K, y*K) for x, y in outer + inner], fill=255)
save(img, sys.argv[1])

# two quavers beamed together
img = Image.new('L', (W, W), 0); d = ImageDraw.Draw(img)
head(d, 135, 412, 78, 56)
head(d, 372, 372, 78, 56)
stem(d, 207, 108, 400, 28)
stem(d, 444, 68, 360, 28)
d.polygon([(179*K, 108*K), (444*K, 68*K), (444*K, 138*K), (179*K, 178*K)], fill=255)
save(img, sys.argv[2])
