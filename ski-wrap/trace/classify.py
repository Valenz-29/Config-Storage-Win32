import numpy as np, sys
from PIL import Image, ImageDraw
d = sys.argv[1]
rect = np.load(d + '/rect.npy') / 255.0
R, G, B = rect[..., 0], rect[..., 1], rect[..., 2]
mx, mn = rect.max(-1), rect.min(-1)
V = mx; S = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
h = np.zeros_like(V)
c = mx - mn + 1e-9
h = np.where(mx == R, ((G - B) / c) % 6, h)
h = np.where(mx == G, (B - R) / c + 2, h)
h = np.where(mx == B, (R - G) / c + 4, h)
h = h * 60
cls = np.full(V.shape, 5, np.uint8)  # 5 = unknown
cls[(S < 0.22) & (V > 0.52)] = 0                        # white
cls[(V < 0.30)] = 1                                     # black
cls[(S > 0.25) & (h > 85) & (h < 175) & (V > 0.25)] = 2 # green
cls[(S > 0.42) & ((h < 30) | (h > 340)) & (V > 0.38)] = 3  # orange
cls[(S > 0.45) & (h > 200) & (h < 250) & (V > 0.34)] = 4   # blue
# Siding (beige, low saturation, warm) is outside the board.
cls[(h > 25) & (h < 65) & (S > 0.07) & (S < 0.35) & (V > 0.45) & (V < 0.9)] = 5
np.save(d + '/cls_raw.npy', cls)
pal = np.array([[246,246,241],[18,19,21],[30,158,74],[240,100,42],[31,79,214],[255,0,200]], np.uint8)
Image.fromarray(pal[cls]).resize((300, 1500), Image.NEAREST).save(d + '/cls_raw.png')
print({k: int((cls == k).sum()) for k in range(6)})
