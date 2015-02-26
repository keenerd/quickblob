#!/usr/bin/env python

import sys
from PIL import Image, ImageDraw
from math import sqrt, pi, ceil, floor

def bb(x, y, r):
    "bounding box for a circle"
    return x-r, y-r, x+r, y+r

if len(sys.argv) != 4:
    print("csv-blobs -t 128 --bbox lorem.png > data.csv")
    print("show-blobs.py data.csv lorem.png blobs.png")
    sys.exit(2)

data   = sys.argv[1]
source = sys.argv[2]
output = sys.argv[3]

img = Image.open(source)
img = img.convert("RGB")
draw = ImageDraw.Draw(img)
height = img.size[1]

for line in open(data):
    line = line.strip().split(',')
    x,y,a,c = line[:4]
    bbox = None
    if len(line) == 8:
        bbox = line[4:]

    """
    if c == 'white':
        gray = 0x95
    elif c == 'black':
        gray = 0x65
    else:
        continue
    """
    if c == 'color':
        continue
    if c == '-1':
        continue
    gray = "rgb(%s,%s,128)" % (c, c)

    try:
        x,y,a = map(float, (x,y,a))
    except ValueError:
        continue

    r = int(sqrt(a/pi) + 1.5)
    if bbox:
        bbox = [int(b) for b in bbox]
        draw.rectangle(bbox, outline=gray, fill=None)
    draw.ellipse(bb(ceil(x),  ceil(y),  r), outline=gray, fill=None)
    draw.ellipse(bb(ceil(x),  floor(y), r), outline=gray, fill=None)
    draw.ellipse(bb(floor(x), ceil(y),  r), outline=gray, fill=None)
    draw.ellipse(bb(floor(x), floor(y), r), outline=gray, fill=None)

img.save(output)

