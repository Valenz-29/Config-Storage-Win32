// home.cpp - Pantalla de inicio: escribir, calcular (ENTER), historial (2nd ENTRY),
// RCL, borrar y mostrar resultados alineados a la derecha como la TI.
#include "ti_int.h"

Editor home_ed;
static char rows[8][17];
static int nrows = 0;

#define HIST 10
static uint8_t hist[HIST][MAXTOK];
static int hist_n[HIST];
static int nhist = 0, hist_idx = -1;
static bool rcl_mode = false;
static bool pending_echo = false;       // la entrada con error se muestra al salir con Quit

void home_clear(void) { nrows = 0; }
void home_clear_entries(void) { nhist = 0; hist_idx = -1; }

static void push_row(const char* s, int n, bool right) {
  if (nrows == 8) { for (int i = 1; i < 8; i++) m_copy(rows[i - 1], rows[i], 17); nrows--; }
  char* r = rows[nrows++];
  for (int i = 0; i < 16; i++) r[i] = ' ';
  r[16] = 0;
  if (n > 16) n = 16;
  int off = right ? 16 - n : 0;
  for (int i = 0; i < n; i++) r[off + i] = s[i];
}

void home_message(const char* right_text) { push_row(right_text, s_len(right_text), true); }

static void echo_entry(void) {
  char buf[600]; int n = ed_text(home_ed, buf, sizeof buf, 0);
  for (int i = 0; i < n; i += 16) push_row(buf + i, n - i < 16 ? n - i : 16, false);
  if (n == 0) push_row("", 0, false);
}

static void save_history(void) {
  if (!home_ed.n) return;
  for (int i = HIST - 1; i > 0; i--) { m_copy(hist[i], hist[i - 1], hist_n[i - 1]); hist_n[i] = hist_n[i - 1]; }
  m_copy(hist[0], home_ed.t, home_ed.n); hist_n[0] = home_ed.n;
  if (nhist < HIST) nhist++;
}

static void execute(void) {
  bool echo = true;
  if (home_ed.n == 0) {                       // ENTER solo repite la última entrada
    if (!nhist) return;
    ed_load(home_ed, hist[0], hist_n[0]);
    echo = false;
  } else save_history();
  hist_idx = -1; ins_mode = false;
  Stmt st; int pos = 0;
  int err = eval_line(home_ed.t, home_ed.n, &st, &pos);
  if (err) { pending_echo = echo; show_error(err, SC_HOME, pos); return; }
  if (echo) echo_entry();
  char out[32]; int n = 0;
  if (st.done) { s_cpy(out, "Done", 8); n = 4; }
  else {
    if (st.conv == 1) n = fmt_frac(st.v, out, 16);
    else if (st.conv == 3) n = fmt_dms(st.v, out, 16);
    if (!n) n = fmt_num(st.v, out, 16);
  }
  push_row(out, n, true);
  ed_clear(home_ed);
}

// Volver del error: pos >= 0 -> Goto (cursor en el error); -1 -> Quit
void home_goto(int pos) {
  if (pos < 0) {
    if (pending_echo) echo_entry();
    pending_echo = false;
    ed_clear(home_ed);
    return;
  }
  pending_echo = false;
  home_ed.cur = pos > home_ed.n ? home_ed.n : pos;
  go(SC_HOME);
}

static void paste_number(double v) {
  uint8_t t[24]; int n = num_tokens(v, t, 24);
  for (int i = 0; i < n; i++) ed_put(home_ed, t[i]);
}

void home_key(int k, int mod) {
  Editor& e = home_ed;
  if (rcl_mode) {
    rcl_mode = false;
    int t = key_token(k, mod);
    if (t >= T_VA && t <= T_VTHETA) paste_number(S.vars[t - T_VA]);
    else if (t == T_ANS) paste_number(S.ans);
    return;
  }
  if (mod == MOD_2ND) {
    switch (k) {
      case K_STO: rcl_mode = true; return;
      case K_DEL: ins_mode = true; return;
      case K_LEFT: e.cur = 0; ins_mode = false; return;
      case K_RIGHT: e.cur = e.n; ins_mode = false; return;
      case K_ENTER:                                     // ENTRY: entradas anteriores
        if (!nhist) return;
        hist_idx = (hist_idx + 1) % nhist;
        ed_load(e, hist[hist_idx], hist_n[hist_idx]);
        return;
    }
  }
  if (mod == MOD_NONE) {
    switch (k) {
      case K_ENTER: execute(); return;
      case K_CLEAR: if (e.n) ed_clear(e); else home_clear(); ins_mode = false; return;
      case K_DEL: ed_del(e); return;
      case K_LEFT: if (e.cur > 0) e.cur--; ins_mode = false; return;
      case K_RIGHT: if (e.cur < e.n) e.cur++; ins_mode = false; return;
      case K_UP: ed_vert(e, -1, 0, 0); ins_mode = false; return;
      case K_DOWN: ed_vert(e, 1, 0, 0); ins_mode = false; return;
    }
  }
  int t = key_token(k, mod);
  if (t >= 0) {
    int kind = tok((uint8_t)t).kind;
    if (e.n == 0 && (kind == TK_BIN || kind == TK_POST || t == T_STO)) ed_put(e, T_ANS);
    ed_put(e, (uint8_t)t);
    return;
  }
  nav_key(k, mod);
}

void home_draw(void) {
  lcd_clear();
  int erows = ed_rows(home_ed, 0, 0);
  int extra = rcl_mode ? 1 : 0;
  int maxe = 8 - extra;
  int skip = 0;
  if (erows > maxe) {                         // entrada muy larga: mostrar la parte con el cursor
    char buf[600]; int start[MAXTOK + 1]; ed_text(home_ed, buf, sizeof buf, start);
    int crow = start[home_ed.cur < home_ed.n ? home_ed.cur : home_ed.n] / 16;
    skip = crow - maxe + 1; if (skip < 0) skip = 0;
    if (skip > erows - maxe) skip = erows - maxe;
    erows = maxe;
  }
  int show = 8 - erows - extra; if (show > nrows) show = nrows; if (show < 0) show = 0;
  int y = 0;
  for (int i = nrows - show; i < nrows; i++) lcd_str(0, y++, rows[i]);
  ed_draw(home_ed, 0, y, 0, erows, !rcl_mode, skip);
  if (rcl_mode) {
    lcd_str(0, 7, "Rcl ");
    if (blink) draw_cursor(4, 7, ' ');
  }
}
