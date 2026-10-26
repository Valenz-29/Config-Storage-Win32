# Fill the binding holes by pairing color runs on the row above and below each hole
# (sequence alignment) and bending every boundary between them with a cubic Hermite curve.
import numpy as np, sys
from PIL import Image, ImageDraw
from scipy import ndimage as ndi
d = sys.argv[1]
cls = np.load(d + '/cls_masked.npy').copy()
H, W = cls.shape
UNK = 5
left, right = np.load(d + '/edges_lr.npy')
cols = np.arange(W)

def poly_mask(pts):
    m = Image.new('L', (W, H), 0)
    ImageDraw.Draw(m).polygon([(x * 2, y * 2) for x, y in pts], fill=1)
    return np.asarray(m).astype(bool)

# Outside the outline: extend edge colors sideways (nearest known pixel in the row).
holes = [(905, 1600), (1912, 2392)]   # the front hole also covers the strap below the binding
hole_rows = np.zeros(H, bool)
for a, b in holes: hole_rows[a:b] = True
strap = poly_mask([(106, 676), (150, 660), (300, 726), (300, 794), (250, 777), (150, 716), (110, 704)])  # inside the front hole
for r in range(H):
    if hole_rows[r]: continue
    row = cls[r]; unk = (row == UNK) & ~strap[r]
    if unk.all(): continue
    if unk.any():
        kn = np.nonzero(~unk & (row != UNK))[0]
        if len(kn):
            pos = np.nonzero(unk)[0]
            nearest = kn[np.clip(np.searchsorted(kn, pos), 0, len(kn) - 1)]
            nearest2 = kn[np.clip(np.searchsorted(kn, pos) - 1, 0, len(kn) - 1)]
            pick = np.where(np.abs(nearest - pos) < np.abs(nearest2 - pos), nearest, nearest2)
            row[pos] = row[pick]

def runs(row):
    b = np.nonzero(np.diff(row.astype(int)))[0] + 1
    st = np.concatenate([[0], b]); en = np.concatenate([b, [len(row)]])
    out = [[int(row[s]), int(s), int(e)] for s, e in zip(st, en)]
    # merge slivers into the wider neighbor
    changed = True
    while changed and len(out) > 1:
        changed = False
        for i, (c, s, e) in enumerate(out):
            if e - s < 4:
                j = i - 1 if i > 0 and (i == len(out) - 1 or out[i-1][2]-out[i-1][1] >= out[i+1][2]-out[i+1][1]) else i + 1
                if j < i: out[j][2] = e
                else: out[j][1] = s
                out.pop(i); changed = True
                # re-merge equal neighbors
                k = 0
                while k < len(out) - 1:
                    if out[k][0] == out[k+1][0]: out[k][2] = out[k+1][2]; out.pop(k+1)
                    else: k += 1
                break
    return out

def align(T, B):
    n, m = len(T), len(B)
    INF = 1e18
    D = np.full((n + 1, m + 1), INF); P = np.zeros((n + 1, m + 1), int)
    D[0, 0] = 0
    wt = lambda r: r[2] - r[1]
    ctr = lambda r: (r[1] + r[2]) / 2
    for i in range(n + 1):
        for j in range(m + 1):
            if i and j and T[i-1][0] == B[j-1][0]:
                c = D[i-1, j-1] + abs(ctr(T[i-1]) - ctr(B[j-1])) * 0.6 + abs(wt(T[i-1]) - wt(B[j-1])) * 0.4
                if c < D[i, j]: D[i, j], P[i, j] = c, 1
            if i:
                c = D[i-1, j] + wt(T[i-1]) * 1.0 + 25
                if c < D[i, j]: D[i, j], P[i, j] = c, 2
            if j:
                c = D[i, j-1] + wt(B[j-1]) * 1.0 + 25
                if c < D[i, j]: D[i, j], P[i, j] = c, 3
    i, j, items = n, m, []
    while i or j:
        p = P[i, j]
        if p == 1: items.append((T[i-1][0], T[i-1], B[j-1])); i -= 1; j -= 1
        elif p == 2: items.append((T[i-1][0], T[i-1], None)); i -= 1
        else: items.append((B[j-1][0], None, B[j-1])); j -= 1
    return items[::-1]

def boundary_slopes(rowA, rowB, xs, pairs, k):
    # dx per row for each boundary, measured between rowA (at the hole edge) and rowB (k rows away).
    rb = runs(rowB)
    bx = [(rb[i][0], rb[i+1][0], rb[i][2]) for i in range(len(rb) - 1)]
    out = []
    for x, (cl, cr) in zip(xs, pairs):
        cand = [bb for bb in bx if bb[0] == cl and bb[1] == cr and abs(bb[2] - x) < 60]
        out.append((min(cand, key=lambda bb: abs(bb[2] - x))[2] - x) / k if cand else None)
    return out

out = cls.copy()
for a0, b0 in holes:
    a, b = a0 - 1, b0
    T, B = runs(cls[a]), runs(cls[b])
    items = align(T, B)
    # Interval ends at the top and at the bottom for every aligned item.
    tx, bxs, colors = [], [], []
    pt, pb = 0, 0
    for c, t, bb in items:
        pt = t[2] if t else pt
        pb = bb[2] if bb else pb
        tx.append(pt); bxs.append(pb); colors.append(c)
    tx, bxs = np.array(tx[:-1], float), np.array(bxs[:-1], float)   # internal boundaries
    pairs = list(zip(colors[:-1], colors[1:]))
    k = 18
    mt = boundary_slopes(cls[a], cls[a - k], tx, pairs, -k)   # slope going down at the top edge
    mb = boundary_slopes(cls[b], cls[b + k], bxs, pairs, k)
    mt = np.array([v if v is not None else 0 for v in mt]); mb = np.array([v if v is not None else 0 for v in mb])
    mt, mb = np.clip(mt, -2.5, 2.5), np.clip(mb, -2.5, 2.5)
    h = b - a
    for r in range(a + 1, b):
        t = (r - a) / h
        h00, h10, h01, h11 = 2*t**3 - 3*t**2 + 1, t**3 - 2*t**2 + t, -2*t**3 + 3*t**2, t**3 - t**2
        x = h00 * tx + h10 * h * mt + h01 * bxs + h11 * h * mb
        x = np.maximum.accumulate(np.clip(x, 0, W))
        edges = np.concatenate([[0], np.round(x).astype(int), [W]])
        row = np.empty(W, np.uint8)
        for c, s, e in zip(colors, edges[:-1], edges[1:]):
            row[s:e] = c
        keep = cls[r] != UNK
        out[r] = np.where(keep & (cols >= 566), cls[r], row)
    print('hole', a0, b0, 'runs', len(T), len(B), 'aligned', len(items))

# Any leftover unknown pixels: nearest known pixel.
unk = out == UNK
if unk.any():
    _, (iy, ix) = ndi.distance_transform_edt(unk, return_indices=True)
    out = out[iy, ix]
np.save(d + '/cls_filled.npy', out)
pal = np.array([[246,246,241],[18,19,21],[30,158,74],[240,100,42],[31,79,214]], np.uint8)
Image.fromarray(pal[out]).resize((300, 1500), Image.NEAREST).save(d + '/cls_filled.png')
