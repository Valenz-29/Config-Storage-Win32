# Genera core/font.h: fuente bitmap 6x12 (ASCII + Latin-1) desde DejaVu Sans Mono.
import sys
from PIL import Image, ImageDraw, ImageFont
W, H = 6, 12
font = ImageFont.truetype("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf", 10)
chars = list(range(32, 127)) + list(range(160, 256))
out = ["// Generado por tools/gen_font.py - no editar a mano", "#pragma once", "#include <stdint.h>",
       f"#define FONT_W {W}", f"#define FONT_H {H}",
       "// Glifos 32..126 y 160..255; cada fila es un byte (bits altos = izquierda)",
       "static const uint8_t FONT_DATA[][FONT_H] = {"]
for c in chars:
    img = Image.new("L", (W, H), 0)
    ImageDraw.Draw(img).text((0, -1), chr(c), font=font, fill=255)
    rows = []
    if c == ord("0"):  # el 0 sale igual al 8 a este tamano: dibujarlo a mano
        rows = [0,0x30,0x48,0x48,0x48,0x48,0x48,0x30,0,0,0,0]
        out.append("  {" + ",".join(f"0x{r:02x}" for r in rows) + "}, // '0'"); continue
    for y in range(H):
        b = 0
        for x in range(W):
            if img.getpixel((x, y)) > 90: b |= 0x80 >> x
        rows.append(b)
    out.append("  {" + ",".join(f"0x{r:02x}" for r in rows) + "}, // " + (repr(chr(c)) if c != 92 else "backslash"))
# Glifos propios (codigos 0x80..): simbolos de la TI que no estan en Latin-1
CUSTOM = [
  ("sup -1",  [0,0x08,0xD8,0x08,0x08,0,0,0,0,0,0,0]),
  ("camara",  [0,0,0x20,0xF8,0xD8,0xD8,0xF8,0,0,0,0,0]),
  ("flecha STO", [0,0,0x20,0x10,0xF8,0x10,0x20,0,0,0,0,0]),
  ("raiz",    [0,0x08,0x08,0x10,0x90,0x50,0x20,0x20,0,0,0,0]),
  ("pi",      [0,0,0xF8,0x50,0x50,0x50,0x50,0x48,0,0,0,0]),
  ("E chica", [0,0,0,0x70,0x40,0x60,0x40,0x70,0,0,0,0]),
  ("negativo",[0,0,0,0x70,0,0,0,0,0,0,0,0]),
  ("flecha 2nd",[0,0x20,0x70,0xA8,0x20,0x20,0x20,0x20,0,0,0,0]),
  ("theta",   [0,0x70,0x88,0x88,0xF8,0x88,0x88,0x70,0,0,0,0]),
]
for name, rows in CUSTOM:
    out.append("  {" + ",".join(f"0x{r:02x}" for r in rows) + "}, // " + name)
out.append("};")
out.append(f"#define FONT_CUSTOM_COUNT {len(CUSTOM)}")
open(sys.argv[1], "w").write("\n".join(out) + "\n")
