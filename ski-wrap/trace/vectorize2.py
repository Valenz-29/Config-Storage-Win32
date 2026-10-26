# Per-layer smoothing on a black base: white lobes and each color are smoothed on their own,
# so the black seams between them survive. Then potrace every layer.
import numpy as np, sys, json
from scipy import ndimage as ndi
import potrace
d, up = sys.argv[1], int(sys.argv[2])
cls = np.load(d + '/cls_filled.npy')
SIG = {0: 4.4, 2: 4.0, 3: 4.0, 4: 4.6}
NAMES = {0: 'W', 2: 'G', 3: 'O', 4: 'B'}
def layer(k):
    ink = cls == k
    sdf = np.where(ink, -ndi.distance_transform_edt(ink), ndi.distance_transform_edt(~ink)).astype(np.float32)
    sdf = ndi.gaussian_filter(sdf, SIG[k])
    m = ndi.zoom(sdf, up, order=3) < 0
    lab, n = ndi.label(m)
    if n:
        sizes = ndi.sum(np.ones_like(lab), lab, index=np.arange(1, n + 1))
        m &= ~np.isin(lab, np.nonzero(sizes < 260 * up * up)[0] + 1)
    holes, n = ndi.label(~m)
    if n:
        sizes = ndi.sum(np.ones_like(holes), holes, index=np.arange(1, n + 1))
        m |= np.isin(holes, np.nonzero(sizes < 120 * up * up)[0] + 1)
    return m
def trace(mask):
    paths = potrace.Bitmap(~mask).trace(turdsize=60, alphamax=1.25, opticurve=True, opttolerance=0.5)
    f = lambda p: f'{p.x:.1f} {p.y:.1f}'
    out = []
    for curve in paths:
        s = 'M' + f(curve.start_point)
        for seg in curve:
            s += ('L' + f(seg.c) + 'L' + f(seg.end_point)) if seg.is_corner else ('C' + f(seg.c1) + ' ' + f(seg.c2) + ' ' + f(seg.end_point))
        out.append(s + 'Z')
    return ''.join(out)
H, W = cls.shape[0] * up, cls.shape[1] * up
layers = {NAMES[k]: trace(layer(k)) for k in (0, 2, 3, 4)}
json.dump({'w': W, 'h': H, 'u0': -0.04, 'u1': 1.04, 'layers': layers}, open(d + '/replica.json', 'w'))
cols = {'W': '#F7F7F4', 'G': '#22A447', 'O': '#EB5B2C', 'B': '#1E4FC4'}
svg = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W//4}" height="{H//4}"><rect width="{W}" height="{H}" fill="#141416"/>' + ''.join(f'<path fill="{cols[n]}" fill-rule="evenodd" d="{p}"/>' for n, p in layers.items()) + '</svg>'
open(d + '/replica_preview.svg', 'w').write(svg)
print('sizes KB', {n: len(p) // 1024 for n, p in layers.items()})
