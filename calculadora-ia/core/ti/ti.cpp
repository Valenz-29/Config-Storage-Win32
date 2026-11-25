// ti.cpp - Modo calculadora normal: enciende/apaga, 2nd/ALPHA, navegación entre
// pantallas, pantalla de error (1:Quit 2:Goto) y avisos.
#include "../ti.h"
#include "ti_int.h"

int screen = SC_HOME;
bool blink = true, second = false, alpha = false, alock = false;
static bool on = true;
static uint32_t blink_t = 0;
static int paste_screen = SC_HOME;

void go(int sc) { screen = sc; g_dirty = true; }

void go_home(void) {
  if (screen == SC_FORM && !form_commit()) return;
  ins_mode = false;
  go(SC_HOME);
}

// ------------------------------------------------------------------ pegar desde menús
Editor* paste_editor(void) {
  switch (paste_screen) {
    case SC_YEQ: return yeq_editor();
    case SC_FORM: return form_editor();
    case SC_GRAPH: return graph_editor();
  }
  return &home_ed;
}

static bool needs_ans(uint8_t t) {
  int k = tok(t).kind;
  return k == TK_BIN || k == TK_POST || k == TK_CONV || t == T_STO;
}

void paste_token(uint8_t t) {
  Editor* e = paste_editor();
  int target = paste_screen;
  if (!e) { e = &home_ed; target = SC_HOME; }
  if (target == SC_HOME && e->n == 0 && needs_ans(t)) ed_put(*e, T_ANS);
  ed_put(*e, t);
  go(target);
}

static void open_paste_menu(int m) {
  paste_screen = SC_HOME;
  if (screen == SC_YEQ && yeq_editor()) paste_screen = SC_YEQ;
  else if (screen == SC_FORM && form_editor()) paste_screen = SC_FORM;
  else if (screen == SC_GRAPH && graph_editor()) paste_screen = SC_GRAPH;
  else if (screen == SC_FORM && !form_commit()) return;
  menu_open(m);
}

void catalog_paste_open(void) {
  paste_screen = SC_HOME;
  if (screen == SC_YEQ && yeq_editor()) paste_screen = SC_YEQ;
  else if (screen == SC_FORM && form_editor()) paste_screen = SC_FORM;
  catalog_open();
}

// ------------------------------------------------------------------ avisos (funciones que aún no existen)
static int info_which = 0, info_return = SC_HOME;

void show_info(int which) {
  if (screen == SC_FORM && !form_commit()) return;
  info_which = which;
  info_return = (screen == SC_INFO || screen == SC_ERR || screen == SC_MENU || screen == SC_CATALOG) ? SC_HOME : screen;
  go(SC_INFO);
}

void info_draw(void) {
  lcd_clear();
  if (info_which == INFO_ABOUT) {
    lcd_str(0, 0, "CALCULADORA IA");
    lcd_str(0, 1, "Modo TI-83 Plus");
    lcd_str(0, 2, "v0.2 (simulador)");
    lcd_str(0, 4, "Nucleo propio,");
    lcd_str(0, 5, "luego emulador");
    lcd_str(0, 6, "con tu ROM.");
    return;
  }
  static const char* const NAMES[] = { "STAT", "LIST", "MATRX", "PRGM", "APPS", "DRAW", "LINK",
                                       "STAT PLOT", "COMPLEJOS", "SECUENCIAS", "", "SOLVER", "MEM MGMT",
                                       "GDB", "PICTURE", "STATISTICS", "STRING", "PARAMETRIC", "POLAR",
                                       "ZOOMSTAT" };
  lcd_str(0, 0, NAMES[info_which], true);
  lcd_str(0, 2, "AUN NO ESTA EN");
  lcd_str(0, 3, "EL SIMULADOR.");
  lcd_str(0, 4, "LLEGA CON EL");
  lcd_str(0, 5, "EMULADOR DE TU");
  lcd_str(0, 6, "TI-83 PLUS.");
}

void info_key(int k, int mod) {
  (void)k; (void)mod;
  go(info_return == SC_INFO ? SC_HOME : info_return);   // cualquier tecla vuelve
}

// ------------------------------------------------------------------ pantalla de error
static int err_code = 0, err_ret = -1, err_pos = 0, err_sel = 0;

void show_error(int code, int ret_screen, int ret_pos) {
  err_code = code; err_ret = ret_screen; err_pos = ret_pos; err_sel = 0;
  second = false; if (!alock) alpha = false;
  go(SC_ERR);
}

void err_draw(void) {
  lcd_clear();
  char l[20] = "ERR:";
  s_cpy(l + 4, ERR_NAME[err_code], 13);
  lcd_str(0, 0, l);
  lcd_str(0, 1, "1:", err_sel == 0); lcd_str(2, 1, "Quit");
  if (err_ret >= 0) { lcd_str(0, 2, "2:", err_sel == 1); lcd_str(2, 2, "Goto"); }
}

static void err_choose(int which) {
  if (which == 1 && err_ret >= 0) {
    if (err_ret == SC_HOME) home_goto(err_pos);
    else if (err_ret == SC_YEQ) yeq_goto(err_pos >> 8, err_pos & 255);
    else go(err_ret);
    return;
  }
  if (err_ret == SC_HOME) home_goto(-1);
  ins_mode = false;
  go(SC_HOME);
}

void err_key(int k, int mod) {
  (void)mod;
  if (k == K_1) { err_choose(0); return; }
  if (k == K_2 && err_ret >= 0) { err_choose(1); return; }
  if (k == K_DOWN && err_ret >= 0) { err_sel = 1; return; }
  if (k == K_UP) { err_sel = 0; return; }
  if (k == K_ENTER) { err_choose(err_sel); return; }
  if (k == K_CLEAR || (mod == MOD_2ND && k == K_MODE)) { err_choose(0); return; }
}

// ------------------------------------------------------------------ teclas globales
bool nav_key(int k, int mod) {
  if (mod == MOD_2ND) {
    switch (k) {
      case K_MODE: go_home(); return true;                       // QUIT
      case K_YEQU: show_info(INFO_STATPLOT); return true;
      case K_WINDOW: if (screen == SC_FORM && !form_commit()) return true; form_open(FORM_TBLSET); return true;
      case K_ZOOM: if (screen == SC_FORM && !form_commit()) return true; form_open(FORM_FORMAT); return true;
      case K_TRACE: if (screen == SC_FORM && !form_commit()) return true; menu_open(MENU_CALC); return true;
      case K_GRAPH: if (screen == SC_FORM && !form_commit()) return true; table_open(); return true;
      case K_MATH: open_paste_menu(MENU_TEST); return true;
      case K_APPS: open_paste_menu(MENU_ANGLE); return true;
      case K_VARS: open_paste_menu(MENU_DISTR); return true;
      case K_ADD: if (screen == SC_FORM && !form_commit()) return true; menu_open(MENU_MEM); return true;
      case K_0: catalog_paste_open(); return true;
      case K_INV: open_paste_menu(MENU_MATRX); return true;
      case K_STAT: open_paste_menu(MENU_LIST); return true;
      case K_PRGM: open_paste_menu(MENU_DRAW); return true;
      case K_XTTN: show_info(INFO_LINK); return true;
      case K_DOT: show_info(INFO_COMPLEX); return true;
      case K_7: case K_8: case K_9: show_info(INFO_SEQ); return true;
      case K_1: case K_2: case K_3: case K_4: case K_5: case K_6:
      case K_LPAREN: case K_RPAREN: show_info(INFO_LIST); return true;
      case K_MUL: case K_SUB: show_info(INFO_MATRIX); return true;
    }
    return false;
  }
  if (mod == MOD_NONE) {
    switch (k) {
      case K_MODE: if (screen == SC_FORM && !form_commit()) return true; form_open(FORM_MODE); return true;
      case K_YEQU: if (screen == SC_FORM && !form_commit()) return true; ins_mode = false; go(SC_YEQ); return true;
      case K_WINDOW: if (screen == SC_FORM && !form_commit()) return true; form_open(FORM_WINDOW); return true;
      case K_ZOOM: if (screen == SC_FORM && !form_commit()) return true; menu_open(MENU_ZOOM); return true;
      case K_TRACE: if (screen == SC_FORM && !form_commit()) return true; graph_open(G_TRACE); return true;
      case K_GRAPH: if (screen == SC_FORM && !form_commit()) return true; graph_open(G_PLAIN); return true;
      case K_MATH: open_paste_menu(MENU_MATH); return true;
      case K_VARS: open_paste_menu(MENU_VARS); return true;
      case K_APPS: open_paste_menu(MENU_APPS); return true;
      case K_PRGM: open_paste_menu(MENU_PRGM); return true;
      case K_STAT: open_paste_menu(MENU_STAT); return true;
    }
  }
  return false;
}

void show_about(void) { show_info(INFO_ABOUT); }
void show_solver_info(void) { show_info(INFO_SOLVER); }
void show_memmgmt_info(void) { show_info(INFO_MEMMGMT); }

// ------------------------------------------------------------------ interfaz con app.cpp
void ti_init(void) {
  state_defaults(true);
  home_clear(); home_clear_entries(); ed_clear(home_ed);
  graph_reset();
  second = alpha = alock = false; ins_mode = false;
  on = true; screen = SC_HOME;
  g_dirty = true;
}

bool ti_is_on(void) { return on; }

void ti_key(int k) {
  g_dirty = true; blink = true; blink_t = hal_millis();
  if (!on) { if (k == K_ON) { on = true; second = alpha = alock = false; } return; }
  if (k == K_ON) { if (second) { on = false; second = false; alpha = alock = false; } return; }
  if (k == K_2ND) { second = !second; return; }
  if (k == K_ALPHA) {
    if (second) { second = false; alpha = alock = true; }
    else if (alpha) { alpha = alock = false; }
    else alpha = true;
    return;
  }
  int mod = second ? MOD_2ND : alpha ? MOD_ALPHA : MOD_NONE;
  second = false;
  if (alpha && !alock) alpha = false;
  if (alock && mod == MOD_ALPHA && !alpha_of(k) && k != K_0 && k != K_DOT && k != K_NEG && k != K_ADD) {
    mod = MOD_NONE;                                  // teclas sin letra funcionan normal en A-LOCK
    if (k == K_ENTER || k == K_CLEAR || k == K_MODE) alpha = alock = false;
  }
  switch (screen) {
    case SC_HOME: home_key(k, mod); break;
    case SC_ERR: err_key(k, mod); break;
    case SC_MENU: menu_key(k, mod); break;
    case SC_FORM: form_key(k, mod); break;
    case SC_YEQ: yeq_key(k, mod); break;
    case SC_GRAPH: graph_key(k, mod); break;
    case SC_TABLE: table_key(k, mod); break;
    case SC_CATALOG: catalog_key(k, mod); break;
    case SC_INFO: info_key(k, mod); break;
  }
}

void ti_tick(uint32_t now) {
  if (on && now - blink_t >= 500) { blink = !blink; blink_t = now; g_dirty = true; }
}

void ti_draw(void) {
  if (!on) { lcd_clear(); lcd_blit(); return; }
  switch (screen) {
    case SC_HOME: home_draw(); break;
    case SC_ERR: err_draw(); break;
    case SC_MENU: menu_draw(); break;
    case SC_FORM: form_draw(); break;
    case SC_YEQ: yeq_draw(); break;
    case SC_GRAPH: graph_draw(); break;
    case SC_TABLE: table_draw(); break;
    case SC_CATALOG: catalog_draw(); break;
    case SC_INFO: info_draw(); break;
  }
  lcd_blit();
}

// Texto de la pantalla para pruebas automáticas
int ti_debug_text(char* out, int cap) {
  int o = 0;
  for (int r = 0; r < 8 && o < cap - 20; r++) {
    for (int c = 0; c < 16; c++) out[o++] = (char)lcd_chars[r][c];
    out[o++] = '\n';
  }
  for (int i = 0; i < lcd_log_n && o < cap - 1; i++) out[o++] = lcd_log[i];
  out[o] = 0;
  return o;
}
