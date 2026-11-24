// app.cpp - Punto de entrada del núcleo: decide si manda la tecla a la TI normal
// o al modo IA, y detecta la combinación secreta Y= + GRAPH.
#include "gfx.h"
#include "keys.h"
#include "hal.h"
#include "ti.h"
#include "ai.h"

#ifdef __wasm__
#define EXPORT(name) extern "C" __attribute__((export_name(#name)))
#else
#define EXPORT(name) extern "C"
#endif

enum { M_TI, M_AI };
static int mode = M_TI;
static bool down[K_COUNT];
// Y= o GRAPH quedan "en espera" un instante: si llega la otra, es la combinación
// secreta y la TI nunca se entera; si no, se entregan normal al soltarlas.
static int pending = -1;
static uint32_t pending_t = 0;
#define PENDING_MAX_MS 500

static void enter_ai() { mode = M_AI; pending = -1; ai_enter(); g_dirty = true; }
static void exit_ai() { ai_exit(); mode = M_TI; g_dirty = true; }

EXPORT(core_init) void core_init(void) { ti_init(); mode = M_TI; g_dirty = true; }

EXPORT(core_key) void core_key(int k, int is_down) {
  if (k < 0 || k >= K_COUNT) return;
  bool was_down = down[k];
  down[k] = is_down != 0;
  if (is_down && was_down) return;  // repetición del teclado del PC

  if (mode == M_AI) {
    if (!is_down) return;
    if (k == K_ON) exit_ai(); else ai_key(k);
    return;
  }
  // --- modo calculadora normal
  if (is_down && (k == K_YEQU || k == K_GRAPH) && ti_is_on()) {
    int other = k == K_YEQU ? K_GRAPH : K_YEQU;
    if (down[other]) { enter_ai(); return; }       // ¡combinación secreta!
    pending = k; pending_t = hal_millis();
    return;
  }
  if (!is_down) {
    if (k == pending) { pending = -1; ti_key(k); }
    return;
  }
  ti_key(k);
}

EXPORT(core_tick) void core_tick(void) {
  uint32_t now = hal_millis();
  if (pending >= 0 && now - pending_t > PENDING_MAX_MS) { int k = pending; pending = -1; ti_key(k); }
  if (mode == M_TI) ti_tick(now); else ai_tick(now);
  if (g_dirty) {
    g_dirty = false;
    if (mode == M_TI) ti_draw(); else ai_draw();
    g_changed = true;
  }
}

// Devuelve 1 si el framebuffer cambió desde la última llamada.
EXPORT(core_frame_changed) int core_frame_changed(void) { int c = g_changed; g_changed = false; return c; }
EXPORT(core_fb) uint16_t* core_fb(void) { return g_fb; }
EXPORT(core_io) char* core_io(void) { return g_io; }
EXPORT(core_io_cap) int core_io_cap(void) { return IO_CAP; }
EXPORT(core_mode) int core_mode(void) { return mode; }
EXPORT(core_wants_text) int core_wants_text(void) { return mode == M_AI && ai_wants_text(); }
EXPORT(core_system_prompt) const char* core_system_prompt(void) { return AI_SYSTEM_PROMPT; }

// Carácter Unicode escrito con un teclado de PC (solo simulador)
EXPORT(core_input_char) void core_input_char(uint32_t cp) {
  if (mode != M_AI) return;
  uint8_t c = unicode_to_local(cp);
  if (c && c != '\n') ai_char(c);
}

EXPORT(core_on_reply) void core_on_reply(int len, int ok) { ai_on_reply(len, ok); }
EXPORT(core_on_photo) void core_on_photo(int ok) { ai_on_photo(ok); }
EXPORT(core_on_wifi_scan) void core_on_wifi_scan(int len) { ai_on_wifi_scan(len); }
EXPORT(core_on_wifi_result) void core_on_wifi_result(int ok) { ai_on_wifi_result(ok); }
