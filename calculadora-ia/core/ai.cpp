// ai.cpp - Modo IA: chat, cámara (visor -> revisión -> adjuntar) y WiFi.
#include "ai.h"
#include "gfx.h"
#include "keys.h"
#include "hal.h"
#include "svg.h"
#include "ti.h"

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
enum { S_CHAT, S_CAM, S_CAM_BUSY, S_REVIEW, S_WIFI_LIST, S_WIFI_PASS, S_WIFI_CONN, S_IMAGE, S_CONFIRM };
static int screen = S_CHAT;

// ------------------------------------------------------------------ historial del chat
enum { R_USER, R_AI, R_SYS };
#define CHAT_CAP 32000
#define MAXMSG 64
struct Msg { uint16_t off, len; uint8_t role; int16_t img, drw, grf; };   // foto, dibujo SVG y gráfica (o -1)

// Dibujos SVG de la IA
#define DRAW_CAP 24000
struct Draw { uint16_t off, len; };
static char dbuf[DRAW_CAP]; static int dused = 0;
static Draw draws[32]; static int ndraw = 0;
// Gráficas de la IA: funciones de x y ventana
struct Graf { char f[10][80]; uint8_t fl[10]; uint8_t n; bool haswin; double w[4]; };
static Graf grafs[6]; static int ngraf = 0;
static bool switch_to_ti = false;
static char cbuf[CHAT_CAP]; static int cused = 0;
static Msg msgs[MAXMSG]; static int nmsg = 0;
static int scroll = 0;            // líneas desde abajo
static int sel_msg = -1;          // mensaje elegido con las flechas (-1 = ninguno)
static int sel_line = 0;          // primera línea visible dentro del mensaje elegido (si es largo)
static int last_vis = 10;         // líneas de chat visibles en pantalla
static int msg_lines(int m);
static bool view_arm = false;     // se apretó Y= con un mensaje con imagen elegido
static int view_img = -1;         // imagen en pantalla completa
static int view_drw = -1;         // o dibujo en pantalla completa
static int next_img = 0;
static bool waiting = false;
static bool photo_attached = false;

static void drop_oldest() {
  if (!nmsg) return;
  int L = msgs[0].len;
  m_move(cbuf, cbuf + L, cused - L); cused -= L;
  for (int i = 1; i < nmsg; i++) { msgs[i - 1] = msgs[i]; msgs[i - 1].off -= L; }
  nmsg--;
  if (sel_msg >= 0) sel_msg--;
}

static void add_msg(int role, const char* s, int n, int img) {
  if (n > CHAT_CAP * 3 / 4) n = CHAT_CAP * 3 / 4;
  while (nmsg >= MAXMSG || cused + n > CHAT_CAP) drop_oldest();
  m_copy(cbuf + cused, s, n);
  msgs[nmsg].off = (uint16_t)cused; msgs[nmsg].len = (uint16_t)n;
  msgs[nmsg].role = (uint8_t)role; msgs[nmsg].img = (int16_t)img; msgs[nmsg].drw = -1; msgs[nmsg].grf = -1;
  nmsg++; cused += n; scroll = 0; g_dirty = true;
}
static void add_sys(const char* utf8) {
  char tmp[256]; int n = utf8_to_local(utf8, s_len(utf8), tmp, sizeof tmp);
  add_msg(R_SYS, tmp, n, -1);
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
  int img = -1;
  if (photo_attached) { img = next_img++; hal_image_keep(img); }
  add_msg(R_USER, input.b, input.n, img);
  sel_msg = -1;
  int n = local_to_utf8(input.b, input.n, g_io, IO_CAP);
  hal_ai_send(g_io, n, photo_attached ? 1 : 0);
  photo_attached = false;
  input.n = 0; input.b[0] = 0;
  waiting = true; dots = 0;
}

void ai_enter(void) { screen = S_CHAT; k2nd = false; g_dirty = true; }
void ai_exit(void) { if (screen == S_CAM || screen == S_CAM_BUSY || screen == S_REVIEW) hal_camera_stop(); screen = S_CHAT; view_arm = false; }

bool ai_wants_text(void) { return screen == S_CHAT || screen == S_WIFI_PASS; }

void ai_char(uint8_t c) {
  if (screen == S_CHAT) f_put(input, c);
  else if (screen == S_WIFI_PASS) f_put(pass, c);
  g_dirty = true;
}

void ai_key(int k) {
  g_dirty = true; blink = true; blink_t = hal_millis();
  switch (screen) {
    case S_CHAT: {
      bool armed = view_arm; view_arm = false;
      bool sel_ok = sel_msg >= 0 && sel_msg < nmsg;
      bool sel_img = sel_ok && (msgs[sel_msg].img >= 0 || msgs[sel_msg].drw >= 0 || msgs[sel_msg].grf >= 0);
      if (k == K_ENTER && armed && sel_img) {
        Msg& g = msgs[sel_msg];
        if (g.grf >= 0) {                                  // graficar en la calculadora
          Graf& G = grafs[g.grf];
          const char* f[10]; int fl[10];
          for (int i = 0; i < G.n; i++) { f[i] = G.f[i]; fl[i] = G.fl[i]; }
          int bad = ti_load_graph(f, fl, G.n, G.haswin ? G.w : 0);
          if (bad >= 0) { add_sys("No pude graficar esa función en la calculadora."); return; }
          switch_to_ti = true;
          return;
        }
        if (g.drw >= 0) { view_drw = g.drw; view_img = -1; }
        else { view_img = g.img; view_drw = -1; }
        screen = S_IMAGE;
        return;
      }
      if (k == K_ENTER || k == K_GRAPH) { sel_msg = -1; send_message(); return; }
      if (k == K_UP || k == K_DOWN) {                 // elegir mensajes; los largos se leen por páginas
        int page = last_vis > 3 ? last_vis - 2 : 1;
        if (k == K_UP) {
          if (!nmsg) return;
          if (sel_msg >= 0 && sel_line > 0) { sel_line -= page; if (sel_line < 0) sel_line = 0; return; }
          if (sel_msg < 0) sel_msg = nmsg - 1; else if (sel_msg > 0) sel_msg--; else return;
          int c = msg_lines(sel_msg); sel_line = c > last_vis ? c - last_vis : 0;   // se ve su final
        } else {
          if (sel_msg < 0) return;
          int c = msg_lines(sel_msg);
          if (sel_line + last_vis < c) { sel_line += page; if (sel_line > c - last_vis) sel_line = c - last_vis; return; }
          if (++sel_msg >= nmsg) { sel_msg = -1; scroll = 0; } else sel_line = 0;
        }
        return;
      }
      if (k == K_YEQU) {
        if (sel_img) { view_arm = true; return; }      // Y= + ENTER: ver la imagen del mensaje
        if (photo_attached) { photo_attached = false; hal_photo_discard(); }
        return;
      }
      if (k == K_WINDOW) { go_camera(); return; }
      if (k == K_ZOOM) { go_wifi(); return; }
      if (k == K_TRACE) {
        bool any = false;
        for (int i = 0; i < nmsg; i++) if (msgs[i].role != R_SYS) any = true;
        if (any) screen = S_CONFIRM; else { hal_ai_reset(); ai_new_chat(); }
        return;
      }
      if (k == K_CLEAR && sel_msg >= 0 && !input.n) { sel_msg = -1; scroll = 0; return; }
      field_key(input, k);
      return;
    }
    case S_IMAGE:
      screen = S_CHAT;                                  // cualquier tecla vuelve al chat
      return;
    case S_CONFIRM:
      if (k == K_GRAPH) { screen = S_CHAT; hal_ai_reset(); ai_new_chat(); return; }
      if (k == K_YEQU || k == K_CLEAR) { screen = S_CHAT; return; }
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
  nmsg = 0; cused = 0; scroll = 0; waiting = false; sel_msg = -1; view_arm = false;
  hal_image_clear();
  dused = 0; ndraw = 0; ngraf = 0;
  char m[48] = "Nuevo chat con ";
  s_cpy(m + 15, ai_name, sizeof m - 16);
  int n = s_len(m); m[n] = '.'; m[n + 1] = 0;
  add_msg(R_SYS, m, n + 1, -1);
}

// Texto del chat para pruebas automáticas: nombre del asistente y mensajes
int ai_debug_text(char* out, int cap) {
  int o = 0;
  auto put = [&](const char* t, int n) { for (int i = 0; i < n && o < cap - 1; i++) out[o++] = t[i]; };
  put(ai_name, s_len(ai_name)); put("\n", 1);
  for (int m = 0; m < nmsg; m++) {
    put(msgs[m].role == R_USER ? "> " : msgs[m].role == R_AI ? "< " : "# ", 2);
    put(cbuf + msgs[m].off, msgs[m].len);
    if (msgs[m].drw >= 0) put(" [dibujo]", 9);
    if (msgs[m].grf >= 0) put(" [grafica]", 10);
    put("\n", 1);
  }
  char st[32] = "#pantalla:0 sel:";
  st[10] = (char)('0' + screen);
  int sn = 16; int v = sel_msg; if (v < 0) { st[sn++] = '-'; v = 1; }
  st[sn++] = (char)('0' + v % 10);
  st[sn++] = ' '; st[sn++] = 'l'; st[sn++] = 'i'; st[sn++] = 'n'; st[sn++] = ':';
  int L = sel_line < 0 ? 0 : sel_line;
  if (L >= 100) st[sn++] = (char)('0' + L / 100 % 10);
  if (L >= 10) st[sn++] = (char)('0' + L / 10 % 10);
  st[sn++] = (char)('0' + L % 10); st[sn++] = '\n';
  put(st, sn);
  out[o] = 0;
  return o;
}

// ------------------------------------------------------------------ dibujos y gráficas en las respuestas
static int lower_c(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int find_ci(const char* s, int n, const char* w, int from) {
  int L = s_len(w);
  for (int i = from; i + L <= n; i++) {
    int j = 0; while (j < L && lower_c((uint8_t)s[i + j]) == lower_c((uint8_t)w[j])) j++;
    if (j == L) return i;
  }
  return -1;
}
static int cut(char* s, int n, int a, int b) { m_move(s + a, s + b, n - b); return n - (b - a); }

static int extract_svg(char* s, int n, int* drw) {
  int a, b;
  while (svg_find(s, n, &a, &b)) {
    int len = b - a;
    if (*drw < 0 && len < DRAW_CAP) {
      if (dused + len > DRAW_CAP || ndraw >= 32) {            // sin espacio: se olvidan los dibujos viejos
        dused = 0; ndraw = 0;
        for (int i = 0; i < nmsg; i++) msgs[i].drw = -1;
      }
      m_copy(dbuf + dused, s + a, len);
      draws[ndraw].off = (uint16_t)dused; draws[ndraw].len = (uint16_t)len;
      *drw = ndraw++; dused += len;
    }
    n = cut(s, n, a, b);
  }
  return n;
}

static bool read_num(const char*& p, const char* e, double* v) {
  while (p < e && (*p == ' ' || *p == ',' || *p == ';')) p++;
  double sg = 1, r = 0, f = 0; bool any = false;
  if (p < e && (*p == '-' || *p == '+')) { if (*p == '-') sg = -1; p++; }
  while (p < e && *p >= '0' && *p <= '9') { r = r * 10 + (*p++ - '0'); any = true; }
  if (p < e && *p == '.') { p++; f = 0.1; while (p < e && *p >= '0' && *p <= '9') { r += (*p++ - '0') * f; f /= 10; any = true; } }
  *v = sg * r;
  return any;
}

static int extract_graph(char* s, int n, int* grf) {
  int a = find_ci(s, n, "[GRAFICA]", 0);
  int hl = 9;
  if (a < 0) { a = find_ci(s, n, "[GR\xC1" "FICA]", 0); }
  if (a < 0) return n;
  int b = find_ci(s, n, "[/GRAFICA]", a), bl = 10;
  if (b < 0) { b = find_ci(s, n, "[/GR\xC1" "FICA]", a); }
  if (b < 0) { b = n; bl = 0; }
  Graf& G = grafs[ngraf % 6];
  G.n = 0; G.haswin = false;
  int i = a + hl;
  while (i < b) {
    int e = i; while (e < b && s[e] != '\n') e++;
    const char* p = s + i; int len = e - i;
    while (len && (*p == ' ' || *p == '-' || *p == '*')) { p++; len--; }
    while (len && (p[len - 1] == ' ' || p[len - 1] == '\r')) len--;
    if (len >= 7 && find_ci(p, len, "ventana", 0) == 0) {
      const char* q = p + 7; double w[4]; int k = 0;
      while (k < 4 && read_num(q, p + len, &w[k])) k++;
      if (k == 4) { m_copy(G.w, w, sizeof w); G.haswin = true; }
    } else if (len > 0 && G.n < 10 && len < 80) {
      bool eq = false; for (int j = 0; j < len; j++) if (p[j] == '=') eq = true;
      bool has_x = false; for (int j = 0; j < len; j++) if (p[j] == 'x' || p[j] == 'X') has_x = true;
      if (eq || has_x) { m_copy(G.f[G.n], p, len); G.fl[G.n] = (uint8_t)len; G.n++; }
    }
    i = e + 1;
  }
  if (G.n) { *grf = ngraf % 6; ngraf++; for (int m = 0; m < nmsg; m++) if (msgs[m].grf == *grf) msgs[m].grf = -1; }
  return cut(s, n, a, b + bl > n ? n : b + bl);
}

// quita restos de bloques de código (```svg) y líneas vacías sobrantes
static int tidy(char* s, int n) {
  int o = 0;
  for (int i = 0; i < n; i++) {
    if (s[i] == '`' && i + 2 < n && s[i + 1] == '`' && s[i + 2] == '`') {
      i += 3; while (i < n && s[i] != '\n' && s[i] != ' ') i++;
      i--; continue;
    }
    if (s[i] == '\n' && o >= 2 && s[o - 1] == '\n' && s[o - 2] == '\n') continue;
    s[o++] = s[i];
  }
  while (o && (s[o - 1] == '\n' || s[o - 1] == ' ')) o--;
  int st = 0; while (st < o && (s[st] == '\n' || s[st] == ' ')) st++;
  if (st) { m_move(s, s + st, o - st); o -= st; }
  return o;
}

bool ai_take_switch(void) { bool r = switch_to_ti; switch_to_ti = false; return r; }

void ai_on_reply(int len, int ok) {
  char* tmp = g_io;  // se convierte en el mismo buffer (la salida nunca es más larga)
  int n = utf8_to_local(g_io, len, tmp, IO_CAP);
  int drw = -1, grf = -1;
  if (ok) { n = extract_graph(tmp, n, &grf); n = extract_svg(tmp, n, &drw); n = tidy(tmp, n); }
  if (!n) { const char* d = drw >= 0 ? "(dibujo)" : "(gráfica)"; n = utf8_to_local(d, s_len(d), tmp, 32); }
  add_msg(ok ? R_AI : R_SYS, tmp, n, -1);
  msgs[nmsg - 1].drw = (int16_t)drw;
  if (drw >= 0 && grf >= 0) add_msg(R_AI, "", 0, -1);     // la gráfica va en su propio mensaje
  msgs[nmsg - 1].grf = (int16_t)grf;
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
      add_msg(R_SYS, m, s_len(m), -1);
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
#define MAXLINES 2000
struct Line { uint16_t off; uint8_t len, kind; int16_t msg; };
enum { L_HUSER, L_HAI, L_USER, L_AI, L_SYS, L_PHOTO, L_PHOTO2, L_DRAW, L_GRAF, L_BLANK, L_WAIT };
static Line lines[MAXLINES]; static int nl, cur_msg;

static void push_line(int off, int len, int kind) {
  if (nl < MAXLINES) { lines[nl].off = (uint16_t)off; lines[nl].len = (uint8_t)len; lines[nl].kind = (uint8_t)kind; lines[nl].msg = (int16_t)cur_msg; nl++; }
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
    cur_msg = m;
    if (g.role == R_USER) push_line(0, 0, L_HUSER);
    if (g.role == R_AI) push_line(0, 0, L_HAI);
    if (g.img >= 0) { push_line(0, 0, L_PHOTO); push_line(0, 0, L_PHOTO2); push_line(0, 0, L_PHOTO2); }
    if (g.drw >= 0) { push_line(0, 0, L_DRAW); push_line(0, 0, L_PHOTO2); push_line(0, 0, L_PHOTO2); }
    if (g.grf >= 0) push_line(0, 0, L_GRAF);
    wrap(g.off, g.len, g.role == R_USER ? L_USER : g.role == R_AI ? L_AI : L_SYS, CHAT_COLS);
    cur_msg = -1;
    push_line(0, 0, L_BLANK);
  }
  cur_msg = -1;
  if (waiting) { push_line(0, 0, L_HAI); push_line(0, 0, L_WAIT); }
}

static int msg_lines(int m) {
  build_lines();
  int c = 0;
  for (int i = 0; i < nl; i++) if (lines[i].msg == m) c++;
  return c;
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
  last_vis = vis;
  int max_scroll = nl - vis; if (max_scroll < 0) max_scroll = 0;
  if (sel_msg >= 0) {                                  // que se vea el mensaje elegido
    int fl = -1, ll = -1;
    for (int i = 0; i < nl; i++) if (lines[i].msg == sel_msg) { if (fl < 0) fl = i; ll = i; }
    if (fl >= 0) {
      int cnt = ll - fl + 1, st;
      if (cnt > vis) {                                 // mensaje largo: mostrar la página elegida
        if (sel_line > cnt - vis) sel_line = cnt - vis;
        if (sel_line < 0) sel_line = 0;
        st = fl + sel_line;
      } else {
        st = nl - vis - scroll;
        if (ll >= st + vis) st = ll - vis + 1;
        if (fl < st) st = fl;
      }
      scroll = nl - vis - st;
      if (scroll < 0) scroll = 0;
    }
  }
  if (scroll > max_scroll) scroll = max_scroll;
  int start = nl - vis - scroll; if (start < 0) start = 0;
  if (!nmsg && !waiting) {
    const char* h[] = { "Modo IA activado.", "", "Escribe tu pregunta y pulsa ENTER.",
                        "ALPHA cambia abc/ABC/123.  2nd: á é í ó ú ñ ¿ ¡ ,",
                        "F2: foto   F3: WiFi   F4: nuevo chat",
                        "ON: volver a la calculadora normal" };
    for (int i = 0; i < 6; i++) { char t[80]; utf8_to_local(h[i], s_len(h[i]), t, sizeof t); gfx_str(4, top + i * CH_H, t, i ? C_DIM : ai_color); }
  }
  for (int i = 0; i < vis && start + i < nl; i++) {      // primero el fondo del mensaje elegido
    Line& l = lines[start + i];
    if (sel_msg >= 0 && l.msg == sel_msg && l.kind != L_BLANK) {
      gfx_fill(0, top + i * CH_H - 1, SCR_W, CH_H, rgb(34, 40, 60));
      gfx_fill(1, top + i * CH_H - 1, 2, CH_H, ai_color);
    }
  }
  for (int i = 0; i < vis && start + i < nl; i++) {
    Line& l = lines[start + i];
    int ly = top + i * CH_H;
    switch (l.kind) {
      case L_HUSER: gfx_str(4, ly, "T\xFA:", C_USER); break;
      case L_HAI: { int w = gfx_str(4, ly, ai_name, ai_color); gfx_char(4 + w, ly, ':', ai_color); break; }
      case L_PHOTO:
        if (!hal_image_draw(msgs[l.msg].img, g_fb, SCR_W, 10, ly, 48, 34)) { gfx_char(10, ly, CH_CAM, C_USER); gfx_str(18, ly, "foto", C_USER); }
        else if (l.msg == sel_msg) { char t[40]; utf8_to_local("Y= + ENTER: ver grande", 22, t, sizeof t); gfx_str(64, ly + 11, t, C_DIM); }
        break;
      case L_PHOTO2: break;
      case L_DRAW: {
        Draw& d = draws[msgs[l.msg].drw];
        svg_render(dbuf + d.off, d.len, 10, ly, 46, 34, false);
        if (l.msg == sel_msg) { char t[40]; utf8_to_local("Y= + ENTER: ver grande", 22, t, sizeof t); gfx_str(62, ly + 11, t, C_DIM); }
        break;
      }
      case L_GRAF: {
        Graf& G = grafs[msgs[l.msg].grf];
        char t[60]; int n = utf8_to_local("Gráfica: ", 10, t, sizeof t);
        int fl = G.fl[0] > 30 ? 30 : G.fl[0];
        m_copy(t + n, G.f[0], fl); n += fl;
        if (G.n > 1) { t[n++] = ' '; t[n++] = '+'; t[n++] = (char)('0' + G.n - 1); }
        gfx_text(10, ly, t, n, C_OK);
        if (l.msg == sel_msg) { char h[40]; int hn = utf8_to_local("  Y=+ENTER: graficar", 20, h, sizeof h); gfx_text(10 + n * CH_W, ly, h, hn, C_DIM); }
        break;
      }
      case L_WAIT: { char t[20] = "pensando"; int n = 8; for (int d = 0; d < dots; d++) t[n++] = '.'; gfx_text(10, ly, t, n, C_DIM); break; }
      case L_BLANK: break;
      default:
        gfx_text(10, ly, cbuf + l.off, l.len, l.kind == L_USER ? C_USER : l.kind == L_AI ? C_TEXT : C_WARN);
    }
  }
  if (scroll) gfx_str(SCR_W - 4 - 6 * CH_W, in_top - CH_H - 1, "\x92 m\xE1s", C_DIM);
  bool sel_ok = sel_msg >= 0 && sel_msg < nmsg;
  const char* f1 = photo_attached ? "Quitar" : "";
  if (sel_ok && msgs[sel_msg].grf >= 0) f1 = "Graf(+ENT)";
  else if (sel_ok && (msgs[sel_msg].img >= 0 || msgs[sel_msg].drw >= 0)) f1 = "Ver(+ENT)";
  softkeys(f1, "Foto", "WiFi", "Nuevo", "Enviar");
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

static void draw_image() {
  gfx_fill(0, 0, SCR_W, SCR_H, 0);
  if (view_drw >= 0) { Draw& d = draws[view_drw]; svg_render(dbuf + d.off, d.len, 0, 0, SCR_W, SCR_H, true); return; }
  if (!hal_image_draw(view_img, g_fb, SCR_W, 0, 0, SCR_W, SCR_H)) {
    char t[40]; utf8_to_local("La imagen ya no está disponible.", 32, t, sizeof t);
    gfx_str(20, 110, t, C_TEXT);
  }
}

static void draw_confirm() {
  gfx_fill(0, 0, SCR_W, SCR_H, C_BG);
  draw_chat();
  for (int y = 0; y < 228; y++) for (int x = (y & 1); x < SCR_W; x += 2) g_fb[y * SCR_W + x] = 0;   // oscurecer
  int bx = 40, by = 76, bw = 240, bh = 76;
  gfx_fill(bx, by, bw, bh, C_BAR);
  gfx_fill(bx, by, bw, 2, C_WARN);
  const char* l1 = "¿Borrar el chat actual?";
  const char* l2 = "Se perderán todos los mensajes.";
  const char* l3 = "F5: Sí, borrar    F1: No";
  char t[60]; int n;
  n = utf8_to_local(l1, s_len(l1), t, sizeof t); gfx_text(bx + (bw - n * CH_W) / 2, by + 12, t, n, C_TEXT);
  n = utf8_to_local(l2, s_len(l2), t, sizeof t); gfx_text(bx + (bw - n * CH_W) / 2, by + 32, t, n, C_DIM);
  n = utf8_to_local(l3, s_len(l3), t, sizeof t); gfx_text(bx + (bw - n * CH_W) / 2, by + 54, t, n, C_WARN);
  softkeys("No", "", "", "", "Sí, borrar");
}

void ai_draw(void) {
  switch (screen) {
    case S_IMAGE: draw_image(); break;
    case S_CONFIRM: draw_confirm(); break;
    case S_CHAT: gfx_fill(0, 0, SCR_W, SCR_H, C_BG); draw_chat(); break;
    case S_CAM: case S_CAM_BUSY: case S_REVIEW: draw_camera(); break;
    default: draw_wifi();
  }
}
