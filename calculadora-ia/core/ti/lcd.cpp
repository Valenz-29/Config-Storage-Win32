// lcd.cpp - La pantalla de la TI-83 Plus: 96x64 pixeles, texto grande 16x8 y texto chico.
// Se dibuja ampliada x3 en la TFT de 320x240.
#include "font_ti.h"
#include "ti_int.h"
#include "../font_small.h"

uint8_t lcd_px[LCD_H][LCD_W];
uint8_t lcd_chars[8][16];
char lcd_log[512];
int lcd_log_n = 0;

void lcd_clear(void) {
  for (int y = 0; y < LCD_H; y++) for (int x = 0; x < LCD_W; x++) lcd_px[y][x] = 0;
  for (int r = 0; r < 8; r++) for (int c = 0; c < 16; c++) lcd_chars[r][c] = ' ';
  lcd_log_n = 0; lcd_log[0] = 0;
}

void lcd_set(int x, int y, int v) { if ((unsigned)x < LCD_W && (unsigned)y < LCD_H) lcd_px[y][x] = (uint8_t)(v != 0); }
int  lcd_get(int x, int y) { return ((unsigned)x < LCD_W && (unsigned)y < LCD_H) ? lcd_px[y][x] : 0; }

// Dibuja nrows filas del glifo grande (desde la fila r0) en la posición de pixel (x, y).
static const uint8_t* ti_glyph(uint8_t ch) {
  static int16_t idx[256];
  static bool ready = false;
  if (!ready) {
    for (int i = 0; i < 256; i++) idx[i] = -1;
    for (int i = 0; i < (int)(sizeof TI_FONT / sizeof TI_FONT[0]); i++) idx[TI_FONT[i].ch] = (int16_t)i;
    ready = true;
  }
  return idx[ch] >= 0 ? TI_FONT[idx[ch]].rows : 0;
}

void lcd_glyph_px(int x, int y, uint8_t ch, int r0, int nrows, bool inv) {
  const uint8_t* g = ti_glyph(ch);
  int shift = 0, h = 8;
  if (!g) {           // símbolos sin glifo de la TI: fuente general
    g = gfx_glyph(ch); h = 12;
    shift = (ch == 'g' || ch == 'p' || ch == 'q' || ch == 'y' || ch == 'j' || ch == ',' || ch == ';') ? 2 : 0;
  }
  for (int r = 0; r < nrows; r++) {
    int src = r0 + r + shift;
    uint8_t bits = src < h ? g[src] : 0;
    for (int c = 0; c < 6; c++) {
      int on = (bits & (0x80 >> c)) ? 1 : 0;
      lcd_set(x + c, y + r, inv ? !on : on);
    }
  }
}

void lcd_char(int col, int row, uint8_t ch, bool inv) {
  if (col < 0 || col >= 16 || row < 0 || row >= 8) return;
  lcd_glyph_px(col * 6, row * 8, ch, 0, 8, inv);
  lcd_chars[row][col] = ch;
}

void lcd_text(int col, int row, const char* s, int n, bool inv) {
  for (int i = 0; i < n; i++) lcd_char(col + i, row, (uint8_t)s[i], inv);
}
void lcd_str(int col, int row, const char* s, bool inv) { lcd_text(col, row, s, s_len(s), inv); }

int lcd_small(int x, int y, const char* s, int n, bool inv) {
  for (int i = 0; i < n; i++) {
    uint8_t ch = (uint8_t)s[i];
    const uint8_t* g = FONT_SMALL[ch];
    if (ch != ' ' && !(g[0] | g[1] | g[2] | g[3] | g[4] | g[5])) g = FONT_SMALL['?'];
    for (int r = 0; r < 6; r++)
      for (int c = 0; c < 4; c++) {
        int on = c < 3 && (g[r] & (4 >> c)) ? 1 : 0;
        if (inv) lcd_set(x + i * 4 + c, y + r, !on);
        else if (on) lcd_set(x + i * 4 + c, y + r, 1);
      }
    if (lcd_log_n < (int)sizeof lcd_log - 2) lcd_log[lcd_log_n++] = (char)ch;
  }
  if (lcd_log_n < (int)sizeof lcd_log - 1) { lcd_log[lcd_log_n++] = '|'; lcd_log[lcd_log_n] = 0; }
  return n * 4;
}
int lcd_small_str(int x, int y, const char* s, bool inv) { return lcd_small(x, y, s, s_len(s), inv); }

void lcd_line(int x0, int y0, int x1, int y1, int v) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (int guard = 0; guard < 4000; guard++) {
    lcd_set(x0, y0, v);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void lcd_fill(int x, int y, int w, int h, int v) {
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) lcd_set(x + i, y + j, v);
}
void lcd_invert(int x, int y, int w, int h) {
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) lcd_set(x + i, y + j, !lcd_get(x + i, y + j));
}

// ------------------------------------------------------------------ a la TFT (x3)
#define LCD_X 16
#define LCD_Y 24
#define PIX 3
void lcd_blit(void) {
  uint16_t bezel = 0x2104, bg = rgb(158, 172, 140), px = rgb(28, 34, 30);
  gfx_fill(0, 0, SCR_W, SCR_H, bezel);
  for (int y = 0; y < LCD_H; y++) {
    uint16_t* row = g_fb + (LCD_Y + y * PIX) * SCR_W + LCD_X;
    for (int x = 0; x < LCD_W; x++) {
      uint16_t c = lcd_px[y][x] ? px : bg;
      for (int k = 0; k < PIX; k++) row[x * PIX + k] = c;
    }
    for (int k = 1; k < PIX; k++) m_copy(row + k * SCR_W, row, LCD_W * PIX * 2);
  }
}
