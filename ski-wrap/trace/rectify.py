# Straighten the board in the photo into a (U x V) grid, then save a preview.
import numpy as np, sys
from PIL import Image
src, out = sys.argv[1], sys.argv[2]
im = np.asarray(Image.open(src).convert('RGB')).astype(np.float32)
H, W, _ = im.shape
# Straight contact-to-contact edge lines (sidecut stays inside the grid).
lineL = lambda y: 174 + (271 - 174) * (y - 274) / (1560 - 274)
lineR = lambda y: 522 + (559 - 522) * (y - 274) / (1560 - 274)
Y0, Y1 = 80, 1726                       # nose top and estimated tail end
ys = np.arange(Y0, Y1 + 1, dtype=np.float64)
w = lineR(ys) - lineL(ys)
dv = 1.0 / w                            # perspective: real length per pixel is ~ 1/width
v_of_y = np.concatenate([[0], np.cumsum((dv[1:] + dv[:-1]) / 2)])
v_of_y /= v_of_y[-1]
U, V = 600, 3000
u = np.linspace(-0.04, 1.04, U)
v = np.linspace(0, 1, V)
yv = np.interp(v, v_of_y, ys)          # image row for each v
xL, xR = lineL(yv), lineR(yv)
X = xL[:, None] + u[None, :] * (xR - xL)[:, None]
Yg = np.repeat(yv[:, None], U, axis=1)
def bilinear(img, x, y):
    x0 = np.clip(np.floor(x).astype(int), 0, W - 2); y0 = np.clip(np.floor(y).astype(int), 0, H - 2)
    fx = (x - x0)[..., None]; fy = (y - y0)[..., None]
    a = img[y0, x0]; b = img[y0, x0 + 1]; c = img[y0 + 1, x0]; d = img[y0 + 1, x0 + 1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy
rect = bilinear(im, X, Yg)
np.save(out + '/rect.npy', rect)
np.save(out + '/map.npy', np.stack([X, Yg]))
Image.fromarray(rect.clip(0, 255).astype(np.uint8)).resize((300, 1500)).save(out + '/rect_preview.png')
print('saved', rect.shape)
