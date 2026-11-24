// gfx.h - Framebuffer 320x240 RGB565 (igual que la pantalla TFT ILI9341) y texto.
// El texto usa un juego de caracteres de 8 bits: ASCII + Latin-1 (á é ñ ¿ ...)
// + símbolos propios de la TI en 0x80..0x88 (ver charset abajo).
#pragma once
#include <stdint.h>
#include <stddef.h>

#define SCR_W 320
#define SCR_H 240
#define CH_W 6
#define CH_H 12

// Símbolos propios (dibujados a mano en tools/gen_font.py)
#define CH_INV1  0x80  // ⁻¹
#define CH_CAM   0x81  // ícono de cámara
#define CH_STO   0x82  // →
#define CH_SQRT  0x83  // √
#define CH_PI    0x84  // π
#define CH_EE    0x85  // ᴇ (notación científica)
#define CH_NEG   0x86  // signo negativo de la TI
#define CH_UP    0x87  // ↑ (cursor de 2nd)
#define CH_THETA 0x88  // θ

extern uint16_t g_fb[SCR_W * SCR_H];
extern bool g_dirty;    // hay que redibujar la pantalla completa
extern bool g_changed;  // el framebuffer cambió (la plataforma debe enviarlo a la TFT)

static inline uint16_t rgb(int r, int g, int b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void gfx_fill(int x, int y, int w, int h, uint16_t c);
void gfx_hline(int x, int y, int w, uint16_t c);
const uint8_t* gfx_glyph(uint8_t ch);                 // 12 filas, bit 0x80 = izquierda
void gfx_char(int x, int y, uint8_t ch, uint16_t fg);
int  gfx_text(int x, int y, const char* s, int n, uint16_t fg);   // devuelve el ancho
int  gfx_str(int x, int y, const char* s, uint16_t fg);

// --- utilidades de texto (sin libc, para compilar igual en wasm y ESP32) ---
int  s_len(const char* s);
void s_cpy(char* d, const char* s, int cap);
void m_copy(void* d, const void* s, int n);
void m_move(void* d, const void* s, int n);
// UTF-8 <-> juego de caracteres local de 8 bits
int  utf8_to_local(const char* in, int n, char* out, int cap);
int  local_to_utf8(const char* in, int n, char* out, int cap);
uint8_t unicode_to_local(uint32_t cp);  // 0 = descartar
