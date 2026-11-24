// editor.cpp - Edición de expresiones como en la TI: cursor por token, modo
// sobrescribir por defecto e INS (2nd DEL) para insertar.
#include "ti_int.h"

bool ins_mode = false;

void ed_clear(Editor& e) { e.n = 0; e.cur = 0; e.fresh = false; }

void ed_load(Editor& e, const uint8_t* t, int n) {
  if (n > MAXTOK) n = MAXTOK;
  m_copy(e.t, t, n); e.n = n; e.cur = n; e.fresh = false;
}

bool ed_put(Editor& e, uint8_t t) {
  if (e.fresh) { e.n = 0; e.cur = 0; e.fresh = false; }
  if (e.cur < 0) e.cur = 0;
  if (e.cur < e.n && !ins_mode) { e.t[e.cur++] = t; return true; }
  if (e.n >= MAXTOK) return false;
  m_move(e.t + e.cur + 1, e.t + e.cur, e.n - e.cur);
  e.t[e.cur++] = t; e.n++;
  return true;
}

void ed_del(Editor& e) {
  e.fresh = false;
  if (e.cur < 0 || e.cur >= e.n) return;
  m_move(e.t + e.cur, e.t + e.cur + 1, e.n - e.cur - 1);
  e.n--;
}

void ed_back(Editor& e) { e.fresh = false; if (e.cur > 0) { e.cur--; ed_del(e); } }

int ed_text(const Editor& e, char* out, int cap, int* start) {
  int o = 0;
  for (int i = 0; i < e.n; i++) {
    if (start) start[i] = o;
    const char* s = tok(e.t[i]).text;
    while (*s && o < cap - 1) out[o++] = *s++;
  }
  if (start) start[e.n] = o;
  out[o] = 0;
  return o;
}

// Posición en pantalla del carácter i: la primera fila empieza en col, las demás en cont.
static void pos_of(int i, int col, int cont, int* r, int* c) {
  int w0 = 16 - col;
  if (i < w0) { *r = 0; *c = col + i; return; }
  int w = 16 - cont; i -= w0;
  *r = 1 + i / w; *c = cont + i % w;
}

int ed_rows(const Editor& e, int col, int cont) {
  char buf[600]; int n = ed_text(e, buf, sizeof buf, 0), r, c;
  pos_of(n, col, cont, &r, &c);
  return r + 1;
}

void draw_cursor(int col, int row, uint8_t under) {
  if (second) lcd_char(col, row, CH_UP, true);
  else if (alpha || alock) lcd_char(col, row, 'A', true);
  else if (ins_mode) { lcd_char(col, row, under, false); lcd_fill(col * 6, row * 8 + 7, 5, 1, 1); }
  else lcd_char(col, row, ' ', true);
}

int ed_draw(const Editor& e, int col, int row, int cont, int maxrows, bool cursor, int skip) {
  char buf[600]; int start[MAXTOK + 1];
  int n = ed_text(e, buf, sizeof buf, start);
  int used = 0;
  for (int i = 0; i < n; i++) {
    int r, c; pos_of(i, col, cont, &r, &c);
    r -= skip;
    if (r < 0 || r >= maxrows) continue;
    lcd_char(c, row + r, (uint8_t)buf[i]);
    if (r + 1 > used) used = r + 1;
  }
  int r, c; pos_of(n, col, cont, &r, &c);
  if (r - skip + 1 > used) used = r - skip + 1;
  if (cursor && blink && e.cur >= 0) {
    int ci = start[e.cur < e.n ? e.cur : e.n];
    pos_of(ci, col, cont, &r, &c);
    r -= skip;
    if (r >= 0 && r < maxrows) draw_cursor(c, row + r, e.cur < e.n ? (uint8_t)buf[ci] : ' ');
  }
  return used;
}

// Mueve el cursor una fila arriba (-1) o abajo (+1). Devuelve sin cambios si no hay fila.
void ed_vert(Editor& e, int dir, int col, int cont) {
  char buf[600]; int start[MAXTOK + 1];
  int n = ed_text(e, buf, sizeof buf, start);
  int ci = start[e.cur < e.n ? (e.cur < 0 ? 0 : e.cur) : e.n], r, c;
  pos_of(ci, col, cont, &r, &c);
  int target_r = r + dir;
  if (target_r < 0) return;
  int best = -1;
  for (int i = 0; i <= e.n; i++) {
    int rr, cc; pos_of(start[i], col, cont, &rr, &cc);
    if (rr == target_r && cc <= c) best = i;
    if (rr == target_r && best < 0) best = i;
  }
  int lr, lc; pos_of(n, col, cont, &lr, &lc);
  if (best < 0 && target_r <= lr) best = e.n;
  if (best >= 0) { e.cur = best; e.fresh = false; }
}

// Token que escribe cada tecla (sin modificador, con 2nd o con ALPHA)
int key_token(int k, int mod) {
  if (mod == MOD_ALPHA) {
    uint8_t a = alpha_of(k);
    if (a >= 'A' && a <= 'Z') return T_VA + (a - 'A');
    if (a == CH_THETA) return T_VTHETA;
    switch (k) {
      case K_0: return T_SPACE; case K_DOT: return T_COLON;
      case K_NEG: return T_QMARK; case K_ADD: return T_QUOTE;
    }
    return -1;
  }
  if (mod == MOD_2ND) {
    switch (k) {
      case K_SIN: return T_ASIN; case K_COS: return T_ACOS; case K_TAN: return T_ATAN;
      case K_SQR: return T_SQRT; case K_LOG: return T_TENPOW; case K_LN: return T_EPOW;
      case K_POW: return T_PI;   case K_DIV: return T_E;      case K_COMMA: return T_EE;
      case K_NEG: return T_ANS;
    }
    return -1;
  }
  if (k >= K_7 && k <= K_9) return 7 + k - K_7;
  if (k >= K_4 && k <= K_6) return 4 + k - K_4;
  if (k >= K_1 && k <= K_3) return 1 + k - K_1;
  switch (k) {
    case K_0: return 0;           case K_DOT: return T_DOT;     case K_ADD: return T_ADD;
    case K_SUB: return T_SUB;     case K_MUL: return T_MUL;     case K_DIV: return T_DIV;
    case K_POW: return T_POW;     case K_LPAREN: return T_LP;   case K_RPAREN: return T_RP;
    case K_NEG: return T_NEG;     case K_SQR: return T_SQR;     case K_INV: return T_INV;
    case K_SIN: return T_SIN;     case K_COS: return T_COS;     case K_TAN: return T_TAN;
    case K_LOG: return T_LOG;     case K_LN: return T_LN;       case K_STO: return T_STO;
    case K_COMMA: return T_COMMA; case K_XTTN: return T_VX;
  }
  return -1;
}
