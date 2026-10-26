# Paint logos over as white lobe, and mark bindings, the strap and the background as unknown.
import numpy as np, sys
from PIL import Image, ImageDraw
from scipy import ndimage as ndi
d = sys.argv[1]
cls = np.load(d + '/cls_raw.npy').copy()
Hh, Ww = cls.shape
UNK = 5

def poly_mask(pts):
    m = Image.new('L', (Ww, Hh), 0)
    ImageDraw.Draw(m).polygon([(x * 2, y * 2) for x, y in pts], fill=1)
    return np.asarray(m).astype(bool)

# Logos and wordmark: painted over as white lobe (preview coordinates x2).
for x0, y0, x1, y1 in [(33, 180, 107, 267), (203, 208, 245, 325), (38, 1235, 105, 1322), (197, 1280, 270, 1372)]:
    cls[y0 * 2:y1 * 2, x0 * 2:x1 * 2] = 0

# Board outline: per-row edges from the classification, median-filtered across rows.
known = cls != UNK
left = np.full(Hh, np.nan); right = np.full(Hh, np.nan)
for r in range(Hh):
    run = ndi.binary_opening(known[r], structure=np.ones(15))
    xs = np.nonzero(run)[0]
    if len(xs): left[r], right[r] = xs[0], xs[-1]
left = ndi.median_filter(np.nan_to_num(left, nan=300), size=61)
right = ndi.median_filter(np.nan_to_num(right, nan=300), size=61)
cols = np.arange(Ww)[None, :]
outside = (cols < left[:, None] + 2) | (cols > right[:, None] - 2)
cls[outside] = UNK
np.save(d + '/edges_lr.npy', np.stack([left, right]))

# Bindings and the strap that crosses the board.
cls[905:1316, :] = UNK
cls[poly_mask([(106, 676), (150, 660), (300, 726), (300, 794), (250, 777), (150, 716), (110, 704)])] = UNK
cls[1912:2392, 0:566] = UNK

# Specks (snow, glare): reassign tiny islands to unknown so the fill decides.
for k in range(5):
    lab, n = ndi.label(cls == k)
    sizes = ndi.sum(np.ones_like(lab), lab, index=np.arange(1, n + 1))
    small = np.isin(lab, np.nonzero(sizes < (300 if k == 4 else 90))[0] + 1)
    cls[small] = UNK

np.save(d + '/cls_masked.npy', cls)

