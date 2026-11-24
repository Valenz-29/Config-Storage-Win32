#include "gfx.h"
#include "font.h"

uint16_t g_fb[SCR_W * SCR_H];
bool g_dirty = true;
bool g_changed = true;

void gfx_fill(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCR_W) w = SCR_W - x;
  if (y + h > SCR_H) h = SCR_H - y;
  for (int j = 0; j < h; j++) {
    uint16_t* p = g_fb + (y + j) * SCR_W + x;
    for (int i = 0; i < w; i++) p[i] = c;
  }
}

void gfx_hline(int x, int y, int w, uint16_t c) { gfx_fill(x, y, w, 1, c); }

const uint8_t* gfx_glyph(uint8_t c) {
  int idx;
  if (c >= 32 && c < 127) idx = c - 32;
  else if (c >= 160) idx = 95 + (c - 160);
  else if (c >= 0x80 && c < 0x80 + FONT_CUSTOM_COUNT) idx = 191 + (c - 0x80);
  else idx = '?' - 32;
  return FONT_DATA[idx];
}

void gfx_char(int x, int y, uint8_t c, uint16_t fg) {
  const uint8_t* g = gfx_glyph(c);
  for (int r = 0; r < FONT_H; r++) {
    int py = y + r;
    if (py < 0 || py >= SCR_H || !g[r]) continue;
    for (int col = 0; col < FONT_W; col++) {
      int px = x + col;
      if ((g[r] & (0x80 >> col)) && px >= 0 && px < SCR_W) g_fb[py * SCR_W + px] = fg;
    }
  }
}

int gfx_text(int x, int y, const char* s, int n, uint16_t fg) {
  for (int i = 0; i < n; i++) gfx_char(x + i * CH_W, y, (uint8_t)s[i], fg);
  return n * CH_W;
}

int gfx_str(int x, int y, const char* s, uint16_t fg) { return gfx_text(x, y, s, s_len(s), fg); }

// ---------------------------------------------------------------- texto
int s_len(const char* s) { int n = 0; while (s[n]) n++; return n; }
void s_cpy(char* d, const char* s, int cap) {
  int i = 0;
  for (; s[i] && i < cap - 1; i++) d[i] = s[i];
  d[i] = 0;
}
void m_copy(void* d, const void* s, int n) {
  char* a = (char*)d; const char* b = (const char*)s;
  for (int i = 0; i < n; i++) a[i] = b[i];
}
void m_move(void* d, const void* s, int n) {
  char* a = (char*)d; const char* b = (const char*)s;
  if (a < b) for (int i = 0; i < n; i++) a[i] = b[i];
  else for (int i = n - 1; i >= 0; i--) a[i] = b[i];
}

uint8_t unicode_to_local(uint32_t cp) {
  if (cp == '\n') return '\n';
  if (cp == '\t') return ' ';
  if (cp >= 32 && cp < 127) return (uint8_t)cp;
  if (cp >= 160 && cp < 256) return (uint8_t)cp;
  switch (cp) {
    case 0x03C0: return CH_PI;
    case 0x221A: return CH_SQRT;
    case 0x2192: return CH_STO;
    case 0x03B8: return CH_THETA;
    case 0x2212: case 0x2013: case 0x2014: return '-';
    case 0x2018: case 0x2019: return '\'';
    case 0x201C: case 0x201D: return '"';
    case 0x2026: return '.';
    case 0x2022: return 0xB7;  // ·
    case 0x2248: return '~';
    case 0x2264: return '<';
    case 0x2265: return '>';
    case 0x22C5: return 0xB7;
    case 0x200B: case 0xFE0F: case 0x200D: return 0;  // invisibles
  }
  if (cp >= 0x1F000) return 0;  // emojis: se descartan
  return '?';
}

int utf8_to_local(const char* in, int n, char* out, int cap) {
  int o = 0;
  const uint8_t* p = (const uint8_t*)in;
  int i = 0;
  while (i < n && o < cap - 1) {
    uint32_t cp; uint8_t b = p[i];
    if (b < 0x80) { cp = b; i += 1; }
    else if ((b & 0xE0) == 0xC0 && i + 1 < n) { cp = ((b & 0x1F) << 6) | (p[i+1] & 0x3F); i += 2; }
    else if ((b & 0xF0) == 0xE0 && i + 2 < n) { cp = ((b & 0x0F) << 12) | ((p[i+1] & 0x3F) << 6) | (p[i+2] & 0x3F); i += 3; }
    else if ((b & 0xF8) == 0xF0 && i + 3 < n) { cp = ((b & 0x07) << 18) | ((p[i+1] & 0x3F) << 12) | ((p[i+2] & 0x3F) << 6) | (p[i+3] & 0x3F); i += 4; }
    else { i += 1; continue; }
    // Quita negritas de markdown ("**")
    if (cp == '*' && i < n && p[i] == '*') { i++; continue; }
    if (cp == '\r') continue;
    uint8_t c = unicode_to_local(cp);
    if (c) out[o++] = (char)c;
  }
  out[o] = 0;
  return o;
}

static uint32_t local_to_unicode(uint8_t c) {
  switch (c) {
    case CH_INV1: return 0x207B;  // se completa con ¹ abajo
    case CH_CAM: return 0x1F4F7;
    case CH_STO: return 0x2192;
    case CH_SQRT: return 0x221A;
    case CH_PI: return 0x03C0;
    case CH_EE: return 'E';
    case CH_NEG: return '-';
    case CH_UP: return 0x2191;
    case CH_THETA: return 0x03B8;
  }
  return c;
}

int local_to_utf8(const char* in, int n, char* out, int cap) {
  int o = 0;
  for (int i = 0; i < n; i++) {
    uint32_t cp = local_to_unicode((uint8_t)in[i]);
    char tmp[8]; int k = 0;
    if (cp < 0x80) tmp[k++] = (char)cp;
    else if (cp < 0x800) { tmp[k++] = (char)(0xC0 | (cp >> 6)); tmp[k++] = (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { tmp[k++] = (char)(0xE0 | (cp >> 12)); tmp[k++] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[k++] = (char)(0x80 | (cp & 0x3F)); }
    else { tmp[k++] = (char)(0xF0 | (cp >> 18)); tmp[k++] = (char)(0x80 | ((cp >> 12) & 0x3F)); tmp[k++] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[k++] = (char)(0x80 | (cp & 0x3F)); }
    if ((uint8_t)in[i] == CH_INV1) { tmp[k++] = (char)0xC2; tmp[k++] = (char)0xB9; }  // ¹
    if (o + k >= cap) break;
    for (int j = 0; j < k; j++) out[o++] = tmp[j];
  }
  out[o] = 0;
  return o;
}

// ---------------------------------------------------------------- runtime mínimo (solo wasm)
#ifdef __wasm__
extern "C" {
void* memcpy(void* d, const void* s, size_t n) { m_copy(d, s, (int)n); return d; }
void* memmove(void* d, const void* s, size_t n) { m_move(d, s, (int)n); return d; }
void* memset(void* d, int v, size_t n) { char* p = (char*)d; for (size_t i = 0; i < n; i++) p[i] = (char)v; return d; }
}
#endif
