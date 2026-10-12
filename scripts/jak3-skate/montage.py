"""(AI-assisted) Put screenshots side by side for viewing.

usage: python montage.py <out.png> <scale> <crop x0,y0,x1,y1 as fractions or 'full'> <image>...
"""
import sys

from PIL import Image

out, scale, crop = sys.argv[1], float(sys.argv[2]), sys.argv[3]
tiles = []
for path in sys.argv[4:]:
    im = Image.open(path).convert('RGB')
    w, h = im.size
    if crop != 'full':
        x0, y0, x1, y1 = [float(v) for v in crop.split(',')]
        im = im.crop((int(w * x0), int(h * y0), int(w * x1), int(h * y1)))
    if scale != 1.0:
        im = im.resize((int(im.size[0] * scale), int(im.size[1] * scale)), Image.LANCZOS)
    tiles.append(im)
sheet = Image.new('RGB', (sum(t.size[0] for t in tiles) + 4 * (len(tiles) - 1), max(t.size[1] for t in tiles)), (255, 0, 255))
x = 0
for t in tiles:
    sheet.paste(t, (x, 0))
    x += t.size[0] + 4
sheet.save(out)
print(out, sheet.size)
