// ai.cpp - Modo IA: chat, cámara (visor -> revisión -> adjuntar) y WiFi.
#include "ai.h"
#include "gfx.h"
#include "keys.h"
#include "hal.h"

char g_io[IO_CAP];

// ------------------------------------------------------------------ colores
#define C_BG     rgb(16, 18, 24)
#define C_BAR    rgb(36, 40, 52)
#define C_TEXT   rgb(235, 235, 235)
#define C_DIM    rgb(140, 146, 160)
#define C_USER   rgb(120, 200, 255)
#define C_CLAUDE rgb(217, 119, 87)

// Nombre y color del asistente (Claude o Gemini, lo elige la plataforma)
static char ai_name[20] = "Claude";
static uint16_t ai_color = C_CLAUDE;
#define C_OK     rgb(80, 220, 120)
#define C_WARN   rgb(255, 190, 80)
#define C_SEL    rgb(60, 70, 100)

// ------------------------------------------------------------------ pantallas
enum { S_CHAT, S_CAM, S_CAM_BUSY, S_REVIEW, S_WIFI_LIST, S_WIFI_PASS, S_WIFI_CONN };
static int screen = S_CHAT;

// ------------------------------------------------------------------ historial del chat
enum { R_USER, R_AI, R_SYS };
#define CHAT_CAP 12000
#define MAXMSG 64
struct Msg { uint16_t off, len; uint8_t role, photo; };
static char cbuf[CHAT_CAP]; static int cused = 0;
static Msg msgs[MAXMSG]; static int nmsg = 0;
static int scroll = 0;            // líneas desde abajo
static bool waiting = false;
static bool photo_attached = false;

static void drop_oldest() {
  if (!nmsg) return;
  int L = msgs[0].len;
  m_move(cbuf, cbuf + L, cused - L); cused -= L;
  for (int i = 1; i < nmsg; i++) { msgs[i - 1] = msgs[i]; msgs[i - 1].off -= L; }
  nmsg--;
}

static void add_msg(int role, const char* s, int n, bool photo) {
  if (n > CHAT_CAP / 2) n = CHAT_CAP / 2;
  while (nmsg >= MAXMSG || cused + n > CHAT_CAP) drop_oldest();
  m_copy(cbuf + cused, s, n);
  msgs[nmsg].off = (uint16_t)cused; msgs[nmsg].len = (uint16_t)n;
  msgs[nmsg].role = (uint8_t)role; msgs[nmsg].photo = photo;
  nmsg++; cused += n; scroll = 0; g_dirty = true;
}
static void add_sys(const char* utf8) {
  char tmp[256]; int n = utf8_to_local(utf8, s_len(utf8), tmp, sizeof tmp);
  add_msg(R_SYS, tmp, n, false);
}

// ------------------------------------------------------------------ campos de texto
struct Field { char b[600]; int n; int cap; };
static Field input = { {0}, 0, 600 };
static Field pass = { {0}, 0, 64 };
static int kmode = 0;      // 0 = abc, 1 = ABC, 2 = 123
static bool k2nd = false;
static bool show_pass = true;

static void f_put(Field& f, uint8_t c) { if (f.n < f.cap - 1) { f.b[f.n++] = (char)c; f.b[f.n] = 0; } }

static uint8_t accent(uint8_t c) {
  switch (c) {
    case 'a': return 0xE1; case 'e': return 0xE9; case 'i': return 0xED;
    case 'o': return 0xF3; case 'u': return 0xFA; case 'n': return 0xF1;
    case 'A': return 0xC1; case 'E': return 0xC9; case 'I': return 0xCD;
    case 'O': return 0xD3; case 'U': return 0xDA; case 'N': return 0xD1;
  }
  return c;
}

// Traduce una tecla a un carácter según el modo (abc/ABC/123) y 2nd.
static uint8_t key_char(int k) {
  bool s = k2nd;
  if (kmode == 2) {
    if (k >= K_7 && k <= K_9) return (uint8_t)('7' + k - K_7);
    if (k >= K_4 && k <= K_6) return (uint8_t)('4' + k - K_4);
    if (k >= K_1 && k <= K_3) return (uint8_t)('1' + k - K_1);
    switch (k) {
      case K_0: return '0';    case K_DOT: return '.';    case K_COMMA: return ',';
      case K_ADD: return '+';  case K_SUB: return '-';    case K_MUL: return '*';
      case K_DIV: return '/';  case K_POW: return s ? CH_PI : '^';
      case K_LPAREN: return '('; case K_RPAREN: return ')';
      case K_NEG: return '-';  case K_STO: return '=';    case K_SQR: return s ? CH_SQRT : 0xB2;
      case K_INV: return CH_INV1;
    }
    return 0;
  }
  switch (k) {                      // signos útiles en modo letras
    case K_0: return ' ';
    case K_DOT: return s ? ',' : '.';
    case K_NEG: return s ? 0xBF : '?';   // ¿
    case K_3: return s ? 0xA1 : '!';     // ¡
    case K_ADD: return s ? '\'' : '"';
  }
  uint8_t a = alpha_of(k);
  if (a < 'A' || a > 'Z') return 0;
  if (kmode == 0) a = (uint8_t)(a + 32);
  return s ? accent(a) : a;
}

// Devuelve true si la tecla se usó para editar el campo
static bool field_key(Field& f, int k) {
  if (k == K_2ND) { k2nd = !k2nd; g_dirty = true; return true; }
  if (k == K_ALPHA) { kmode = (kmode + 1) % 3; k2nd = false; g_dirty = true; return true; }
  if (k == K_DEL) { if (f.n) f.b[--f.n] = 0; g_dirty = true; return true; }
  if (k == K_CLEAR) { f.n = 0; f.b[0] = 0; g_dirty = true; return true; }
  uint8_t c = key_char(k);
  if (!c) return false;
  k2nd = false;
  f_put(f, c); g_dirty = true;
  return true;
}

// ------------------------------------------------------------------ WiFi
#define MAXNETS 12
struct Net { char ssid[33]; int rssi; bool secure; };
static Net nets[MAXNETS]; static int nnets = 0, sel = 0;
static bool scanning = false;
static bool wifi_ok = false;
static char wifi_name[33] = "";
static char wifi_msg[64] = "";

// ------------------------------------------------------------------ cámara
static int bright = 0;
static bool cam_ready = false;

// ------------------------------------------------------------------ animación
static bool blink = true; static uint32_t blink_t = 0;
static int dots = 0;

// ------------------------------------------------------------------ acciones
static void go_camera() {
  screen = S_CAM; cam_ready = false; bright = 0;
  hal_camera_brightness(0);
  hal_camera_start();
  g_dirty = true;
}
static void leave_camera() { hal_camera_stop(); screen = S_CHAT; g_dirty = true; }

static void go_wifi() {
  screen = S_WIFI_LIST; scanning = true; nnets = 0; sel = 0;
  hal_wifi_scan(); g_dirty = true;
}

static void send_message() {
  if (waiting) return;
  if (input.n == 0 && !photo_attached) return;
  if (!wifi_ok) { add_sys("Sin conexión. Conéctate a una red con F3 (WiFi)."); return; }
  if (input.n == 0) {
    const char* d = "¿Qué ves en esta imagen?";
    input.n = utf8_to_local(d, s_len(d), input.b, input.cap);
  }
  add_msg(R_USER, input.b, input.n, photo_attached);
  int n = local_to_utf8(input.b, input.n, g_io, IO_CAP);
  hal_ai_send(g_io, n, photo_attached ? 1 : 0);
  photo_attached = false;
  input.n = 0; input.b[0] = 0;
  waiting = true; dots = 0;
}

void ai_enter(void) { screen = S_CHAT; k2nd = false; g_dirty = true; }
void ai_exit(void) { if (screen == S_CAM || screen == S_CAM_BUSY || screen == S_REVIEW) hal_camera_stop(); screen = S_CHAT; }

bool ai_wants_text(void) { return screen == S_CHAT || screen == S_WIFI_PASS; }

void ai_char(uint8_t c) {
  if (screen == S_CHAT) f_put(input, c);
  else if (screen == S_WIFI_PASS) f_put(pass, c);
  g_dirty = true;
}

void ai_key(int k) {
  g_dirty = true; blink = true; blink_t = hal_millis();
  switch (screen) {
    case S_CHAT:
      if (k == K_ENTER || k == K_GRAPH) { send_message(); return; }
      if (k == K_UP) { scroll++; return; }
      if (k == K_DOWN) { if (scroll) scroll--; return; }
      if (k == K_YEQU) { if (photo_attached) { photo_attached = false; hal_photo_discard(); } return; }
      if (k == K_WINDOW) { go_camera(); return; }
      if (k == K_ZOOM) { go_wifi(); return; }
      if (k == K_TRACE) { hal_ai_reset(); ai_new_chat(); return; }
      field_key(input, k);
      return;
    case S_CAM:
      if (k == K_ENTER || k == K_GRAPH) { screen = S_CAM_BUSY; hal_camera_capture(); return; }
      if (k == K_UP && bright < 2) { hal_camera_brightness(++bright); return; }
      if (k == K_DOWN && bright > -2) { hal_camera_brightness(--bright); return; }
      if (k == K_YEQU || k == K_CLEAR) { leave_camera(); return; }
      return;
    case S_REVIEW:
      if (k == K_YEQU || k == K_CLEAR) { hal_photo_discard(); screen = S_CAM; cam_ready = false; return; }
      if (k == K_GRAPH || k == K_ENTER) { photo_attached = true; hal_camera_stop(); screen = S_CHAT; return; }
      return;
    case S_WIFI_LIST:
      if (k == K_YEQU || k == K_CLEAR) { screen = S_CHAT; return; }
      if (k == K_WINDOW) { go_wifi(); return; }
      if (k == K_UP && sel > 0) { sel--; return; }
      if (k == K_DOWN && sel < nnets - 1) { sel++; return; }
      if (k == K_ENTER && nnets) {
        wifi_msg[0] = 0; pass.n = 0; pass.b[0] = 0;
        if (nets[sel].secure) { screen = S_WIFI_PASS; kmode = 0; }
        else { screen = S_WIFI_CONN; hal_wifi_connect(nets[sel].ssid, ""); }
      }
      return;
    case S_WIFI_PASS:
      if (k == K_YEQU) { screen = S_WIFI_LIST; return; }
      if (k == K_WINDOW) { show_pass = !show_pass; return; }
      if (k == K_ENTER || k == K_GRAPH) {
        char p8[200]; local_to_utf8(pass.b, pass.n, p8, sizeof p8);
        screen = S_WIFI_CONN; hal_wifi_connect(nets[sel].ssid, p8);
        return;
      }
      field_key(pass, k);
      return;
  }
}

void ai_set_assistant(const char* utf8, int len, uint16_t color) {
  utf8_to_local(utf8, len, ai_name, sizeof ai_name);
  ai_color = color;
  g_dirty = true;
}

void ai_new_chat(void) {
  nmsg = 0; cused = 0; scroll = 0; waiting = false;
  char m[48] = "Nuevo chat con ";
  s_cpy(m + 15, ai_name, sizeof m - 16);
  int n = s_len(m); m[n] = '.'; m[n + 1] = 0;
  add_msg(R_SYS, m, n + 1, false);
}

// Texto del chat para pruebas automáticas: nombre del asistente y mensajes
int ai_debug_text(char* out, int cap) {
  int o = 0;
  auto put = [&](const char* t, int n) { for (int i = 0; i < n && o < cap - 1; i++) out[o++] = t[i]; };
  put(ai_name, s_len(ai_name)); put("\n", 1);
  for (int m = 0; m < nmsg; m++) {
    put(msgs[m].role == R_USER ? "> " : msgs[m].role == R_AI ? "< " : "# ", 2);
    put(cbuf + msgs[m].off, msgs[m].len); put("\n", 1);
  }
  out[o] = 0;
  return o;
}

void ai_on_reply(int len, int ok) {
  char* tmp = g_io;  // se convierte en el mismo buffer (la salida nunca es más larga)
  int n = utf8_to_local(g_io, len, tmp, IO_CAP);
  add_msg(ok ? R_AI : R_SYS, tmp, n, false);
  waiting = false;
}

void ai_on_photo(int ok) {
  if (screen != S_CAM_BUSY) return;
  screen = ok ? S_REVIEW : S_CAM;
  g_dirty = true;
}

void ai_on_wifi_scan(int len) {
  nnets = 0; scanning = false;
  int i = 0;
  while (i < len && nnets < MAXNETS) {
    Net& w = nets[nnets];
    int j = 0;
    while (i < len && g_io[i] != '\t' && g_io[i] != '\n') { if (j < 32) w.ssid[j++] = g_io[i]; i++; }
    w.ssid[j] = 0;
    w.rssi = 0; w.secure = false;
    if (i < len && g_io[i] == '\t') {
      i++; bool neg = false; if (g_io[i] == '-') { neg = true; i++; }
      while (i < len && g_io[i] >= '0' && g_io[i] <= '9') w.rssi = w.rssi * 10 + (g_io[i++] - '0');
      if (neg) w.rssi = -w.rssi;
    }
    if (i < len && g_io[i] == '\t') { i++; w.secure = g_io[i] == '1'; }
    while (i < len && g_io[i] != '\n') i++;
    i++;
    if (j) nnets++;
  }
  sel = 0; g_dirty = true;
}

void ai_on_wifi_result(int ok) {
  if (ok) {
    wifi_ok = true;
    utf8_to_local(g_io, s_len(g_io), wifi_name, sizeof wifi_name);
    if (screen == S_WIFI_CONN) {
      char m[80] = "Conectado a ";
      s_cpy(m + 12, wifi_name, 60);
      add_msg(R_SYS, m, s_len(m), false);
      screen = S_CHAT;
    }
  } else {
    wifi_ok = false;
    if (screen == S_WIFI_CONN) {
      utf8_to_local("No se pudo conectar. ¿Contraseña correcta?", 44, wifi_msg, sizeof wifi_msg);
      screen = nets[sel].secure ? S_WIFI_PASS : S_WIFI_LIST;
    }
  }
  g_dirty = true;
}

static void crosshair() {
  uint16_t w = rgb(255, 255, 255);
  gfx_fill(160 - 8, 120, 17, 1, w); gfx_fill(160, 120 - 8, 1, 17, w);
}

void ai_tick(uint32_t now) {
  if (now - blink_t >= 500) {
    blink = !blink; blink_t = now;
    if (waiting) dots = (dots + 1) % 4;
    if (screen != S_CAM) g_dirty = true;
  }
  if (screen == S_CAM) {
    if (hal_camera_preview(g_fb, SCR_W, 20, 15, 280, 210)) {
      if (!cam_ready) { cam_ready = true; g_dirty = true; }
      crosshair();
      g_changed = true;
    }
  }
}

// ------------------------------------------------------------------ dibujo
static void status_bar(const char* title_utf8, uint16_t dot) {
  gfx_fill(0, 0, SCR_W, 13, C_BAR);
  int x = 4;
  if (dot) { gfx_fill(4, 3, 6, 6, dot); x = 14; }
  char t[60]; utf8_to_local(title_utf8, s_len(title_utf8), t, sizeof t);
  gfx_str(x, 1, t, C_TEXT);
  // derecha: modo de escritura y WiFi
  char r[40]; int n = 0;
  if (ai_wants_text()) {
    const char* m = kmode == 0 ? "abc" : kmode == 1 ? "ABC" : "123";
    if (k2nd) { r[n++] = '2'; r[n++] = 'n'; r[n++] = 'd'; r[n++] = ' '; }
    r[n++] = '['; while (*m) r[n++] = *m++; r[n++] = ']'; r[n++] = ' ';
  }
  const char* w = wifi_ok ? "WiFi" : "sin WiFi";
  while (*w) r[n++] = *w++;
  gfx_text(SCR_W - 4 - n * CH_W, 1, r, n, wifi_ok ? C_OK : C_DIM);
}

static void softkeys(const char* a, const char* b, const char* c, const char* d, const char* e) {
  const char* L[5] = { a, b, c, d, e };
  gfx_fill(0, 228, SCR_W, 12, C_BAR);
  for (int i = 0; i < 5; i++) {
    if (!L[i] || !*L[i]) continue;
    char t[16]; int n = utf8_to_local(L[i], s_len(L[i]), t, sizeof t);
    gfx_text(i * 64 + (64 - n * CH_W) / 2, 228, t, n, C_TEXT);
    if (i) gfx_fill(i * 64, 230, 1, 8, C_DIM);
  }
}

// --- chat
#define CHAT_COLS 52
#define MAXLINES 600
struct Line { uint16_t off; uint8_t len, kind; };
enum { L_HUSER, L_HAI, L_USER, L_AI, L_SYS, L_PHOTO, L_BLANK, L_WAIT };
static Line lines[MAXLINES]; static int nl;

static void push_line(int off, int len, int kind) {
  if (nl < MAXLINES) { lines[nl].off = (uint16_t)off; lines[nl].len = (uint8_t)len; lines[nl].kind = (uint8_t)kind; nl++; }
}

static void wrap(int off, int len, int kind, int cols) {
  int i = 0;
  while (i < len) {
    int end = i, last_space = -1;
    while (end < len && end - i < cols && cbuf[off + end] != '\n') { if (cbuf[off + end] == ' ') last_space = end; end++; }
    if (end < len && cbuf[off + end] != '\n' && end - i == cols && last_space > i) end = last_space;
    push_line(off + i, end - i, kind);
    i = end;
    if (i < len && (cbuf[off + i] == '\n' || cbuf[off + i] == ' ')) i++;
  }
}

static void build_lines() {
  nl = 0;
  for (int m = 0; m < nmsg; m++) {
    Msg& g = msgs[m];
    if (g.role == R_USER) push_line(0, 0, L_HUSER);
    if (g.role == R_AI) push_line(0, 0, L_HAI);
    if (g.photo) push_line(0, 0, L_PHOTO);
    wrap(g.off, g.len, g.role == R_USER ? L_USER : g.role == R_AI ? L_AI : L_SYS, CHAT_COLS);
    push_line(0, 0, L_BLANK);
  }
  if (waiting) { push_line(0, 0, L_HAI); push_line(0, 0, L_WAIT); }
}

static void draw_chat() {
  status_bar(ai_name, ai_color);
  // área de escritura (abajo)
  int in_cols = 50;
  int in_rows = input.n / in_cols + 1; if (in_rows > 3) in_rows = 3;
  int in_h = in_rows * CH_H + (photo_attached ? 34 : 0) + 4;
  int in_top = 228 - in_h;
  gfx_fill(0, in_top, SCR_W, in_h, rgb(26, 29, 38));
  gfx_hline(0, in_top, SCR_W, C_SEL);
  int y = in_top + 2;
  if (photo_attached) {
    hal_photo_draw(g_fb, SCR_W, 4, y, 40, 30);
    char t[40]; utf8_to_local("Foto adjunta (F1: quitar)", 25, t, sizeof t);
    gfx_str(50, y + 9, t, C_DIM);
    y += 34;
  }
  int total_rows = input.n / in_cols + 1;
  int first = (total_rows - in_rows) * in_cols;
  gfx_char(2, y, '>', C_USER);
  for (int i = first, c = 0; i < input.n; i++, c++)
    gfx_char(14 + (c % in_cols) * CH_W, y + (c / in_cols) * CH_H, (uint8_t)input.b[i], C_TEXT);
  if (blink) { int c = input.n - first; gfx_char(14 + (c % in_cols) * CH_W, y + (c / in_cols) * CH_H, '_', C_USER); }
  // mensajes
  build_lines();
  int top = 15, vis = (in_top - 2 - top) / CH_H;
  int max_scroll = nl - vis; if (max_scroll < 0) max_scroll = 0;
  if (scroll > max_scroll) scroll = max_scroll;
  int start = nl - vis - scroll; if (start < 0) start = 0;
  if (!nmsg && !waiting) {
    const char* h[] = { "Modo IA activado.", "", "Escribe tu pregunta y pulsa ENTER.",
                        "ALPHA cambia abc/ABC/123.  2nd: á é í ó ú ñ ¿ ¡ ,",
                        "F2: foto   F3: WiFi   F4: nuevo chat",
                        "ON: volver a la calculadora normal" };
    for (int i = 0; i < 6; i++) { char t[80]; utf8_to_local(h[i], s_len(h[i]), t, sizeof t); gfx_str(4, top + i * CH_H, t, i ? C_DIM : ai_color); }
  }
  for (int i = 0; i < vis && start + i < nl; i++) {
    Line& l = lines[start + i];
    int ly = top + i * CH_H;
    switch (l.kind) {
      case L_HUSER: gfx_str(4, ly, "T\xFA:", C_USER); break;
      case L_HAI: { int w = gfx_str(4, ly, ai_name, ai_color); gfx_char(4 + w, ly, ':', ai_color); break; }
      case L_PHOTO: gfx_char(10, ly, CH_CAM, C_USER); gfx_str(18, ly, "foto", C_USER); break;
      case L_WAIT: { char t[20] = "pensando"; int n = 8; for (int d = 0; d < dots; d++) t[n++] = '.'; gfx_text(10, ly, t, n, C_DIM); break; }
      case L_BLANK: break;
      default:
        gfx_text(10, ly, cbuf + l.off, l.len, l.kind == L_USER ? C_USER : l.kind == L_AI ? C_TEXT : C_WARN);
    }
  }
  if (scroll) gfx_str(SCR_W - 4 - 6 * CH_W, in_top - CH_H - 1, "\x87 m\xE1s", C_DIM);
  softkeys(photo_attached ? "Quitar" : "", "Foto", "WiFi", "Nuevo", "Enviar");
}

static void draw_camera() {
  gfx_fill(0, 0, SCR_W, SCR_H, 0);
  if (screen == S_REVIEW) {
    status_bar("¿Usar esta foto?", C_WARN);
    hal_photo_draw(g_fb, SCR_W, 20, 15, 280, 210);
    softkeys("Repetir", "", "", "", "Usar");
    return;
  }
  if (screen == S_CAM_BUSY) {
    status_bar("Sacando foto...", C_WARN);
    hal_camera_preview(g_fb, SCR_W, 20, 15, 280, 210);
    softkeys("", "", "", "", "");
    return;
  }
  status_bar(cam_ready ? "CÁMARA LISTA" : "Iniciando cámara...", cam_ready ? C_OK : C_DIM);
  if (bright) {
    char t[12] = "Brillo +0"; t[7] = bright > 0 ? '+' : '-'; t[8] = (char)('0' + (bright > 0 ? bright : -bright));
    gfx_str(150, 1, t, C_TEXT);
  }
  if (hal_camera_preview(g_fb, SCR_W, 20, 15, 280, 210)) crosshair();
  softkeys("Volver", "", "\x87/v Brillo", "", "Sacar");
}

static void draw_wifi() {
  gfx_fill(0, 0, SCR_W, SCR_H, C_BG);
  if (screen == S_WIFI_LIST) {
    status_bar("WiFi", 0);
    if (scanning) gfx_str(10, 30, "Buscando redes...", C_DIM);
    else if (!nnets) gfx_str(10, 30, "No se encontraron redes.", C_DIM);
    for (int i = 0; i < nnets; i++) {
      int y = 20 + i * 16;
      if (i == sel) gfx_fill(4, y - 2, SCR_W - 8, 15, C_SEL);
      char s[40]; utf8_to_local(nets[i].ssid, s_len(nets[i].ssid), s, sizeof s);
      gfx_str(10, y, s, C_TEXT);
      bool cur = wifi_ok && s_len(wifi_name) && s_len(s) == s_len(wifi_name);
      for (int j = 0; cur && s[j]; j++) if (s[j] != wifi_name[j]) cur = false;
      if (cur) gfx_str(10 + (s_len(s) + 1) * CH_W, y, "(conectado)", C_OK);
      int bars = nets[i].rssi > -55 ? 4 : nets[i].rssi > -67 ? 3 : nets[i].rssi > -78 ? 2 : 1;
      for (int b = 0; b < 4; b++) gfx_fill(SCR_W - 60 + b * 5, y + 9 - b * 2, 3, 2 + b * 2, b < bars ? C_TEXT : C_BAR);
      if (nets[i].secure) gfx_str(SCR_W - 34, y, "clave", C_DIM);
    }
    softkeys("Volver", "Buscar", "", "", "");
    return;
  }
  if (screen == S_WIFI_CONN) {
    status_bar("WiFi", 0);
    char s[40]; utf8_to_local(nets[sel].ssid, s_len(nets[sel].ssid), s, sizeof s);
    gfx_str(10, 40, "Conectando a", C_DIM); gfx_str(10 + 13 * CH_W, 40, s, C_TEXT);
    char t[8] = "..."; t[(hal_millis() / 400) % 4] = 0; gfx_str(10, 56, t, C_TEXT);
    softkeys("", "", "", "", "");
    return;
  }
  // contraseña
  status_bar("Contraseña WiFi", 0);
  char s[40]; utf8_to_local(nets[sel].ssid, s_len(nets[sel].ssid), s, sizeof s);
  gfx_str(10, 24, "Red:", C_DIM); gfx_str(40, 24, s, C_TEXT);
  gfx_str(10, 48, "Contrase\xF1" "a:", C_DIM);
  gfx_fill(8, 62, SCR_W - 16, 16, rgb(26, 29, 38));
  for (int i = 0; i < pass.n && i < 50; i++) gfx_char(12 + i * CH_W, 64, show_pass ? (uint8_t)pass.b[i] : '*', C_TEXT);
  if (blink) gfx_char(12 + (pass.n < 50 ? pass.n : 50) * CH_W, 64, '_', C_USER);
  if (wifi_msg[0]) gfx_str(10, 90, wifi_msg, C_WARN);
  char h[80]; utf8_to_local("ALPHA: abc/ABC/123   DEL: borrar", 32, h, sizeof h); gfx_str(10, 200, h, C_DIM);
  softkeys("Volver", show_pass ? "Ocultar" : "Ver", "", "", "Conectar");
}

void ai_draw(void) {
  switch (screen) {
    case S_CHAT: gfx_fill(0, 0, SCR_W, SCR_H, C_BG); draw_chat(); break;
    case S_CAM: case S_CAM_BUSY: case S_REVIEW: draw_camera(); break;
    default: draw_wifi();
  }
}
