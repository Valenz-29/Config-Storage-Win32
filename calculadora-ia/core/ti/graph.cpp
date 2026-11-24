// graph.cpp - Gráficas como en la TI-83 Plus: editor Y=, dibujo en 95x63 pixeles,
// cursor libre, TRACE, CALC, ZOOM y la tabla (TABLE).
#include "ti_int.h"

// ===================================================================== editor Y=
static int yf = 0;      // función con el cursor (0..9 = Y1..Y9, Y0)

Editor* yeq_editor(void) { if (S.y[yf].cur < 0) S.y[yf].cur = 0; return &S.y[yf]; }
void yeq_goto(int fn, int pos) { yf = fn < 0 || fn > 9 ? 0 : fn; S.y[yf].cur = pos; go(SC_YEQ); }

static int y_cursor_row(int f) {   // fila (dentro de la función) donde está el cursor
  Editor& e = S.y[f];
  char buf[600]; int st[MAXTOK + 1]; ed_text(e, buf, sizeof buf, st);
  int ci = st[e.cur < 0 ? 0 : (e.cur < e.n ? e.cur : e.n)];
  return ci < 12 ? 0 : 1 + (ci - 12) / 12;
}

void yeq_key(int k, int mod) {
  Editor& e = S.y[yf];
  if (mod == MOD_2ND) {
    if (k == K_DEL) { ins_mode = true; return; }
    if (k == K_LEFT) { e.cur = 0; return; }
    if (k == K_RIGHT) { e.cur = e.n; return; }
  }
  if (mod == MOD_NONE) {
    switch (k) {
      case K_UP:
        ins_mode = false;
        if (e.cur >= 0 && y_cursor_row(yf) > 0) { ed_vert(e, -1, 4, 4); return; }
        if (yf > 0) { yf--; S.y[yf].cur = 0; }
        return;
      case K_DOWN: case K_ENTER:
        ins_mode = false;
        if (k == K_ENTER && e.cur < 0) { S.ysel[yf] = !S.ysel[yf]; graph_dirty = true; return; }
        if (k == K_DOWN && e.cur >= 0 && y_cursor_row(yf) < ed_rows(e, 4, 4) - 1) { ed_vert(e, 1, 4, 4); return; }
        if (yf < 9) { yf++; S.y[yf].cur = 0; }
        return;
      case K_LEFT: ins_mode = false; if (e.cur > -1) e.cur--; return;
      case K_RIGHT: ins_mode = false; if (e.cur < e.n) e.cur++; return;
      case K_CLEAR: ed_clear(e); graph_dirty = true; return;
      case K_DEL: if (e.cur >= 0) { ed_del(e); graph_dirty = true; } return;
    }
  }
  int t = key_token(k, mod);
  if (t >= 0) {
    if (e.cur < 0) e.cur = 0;
    if (e.n == 0) S.ysel[yf] = 1;
    ed_put(e, (uint8_t)t);
    graph_dirty = true;
    return;
  }
  nav_key(k, mod);
}

void yeq_draw(void) {
  lcd_clear();
  lcd_small_str(1, 1, "Plot1  Plot2  Plot3");
  // primera función visible: la que deja ver el cursor
  static int ytop = 0;
  if (yf < ytop) ytop = yf;
  for (;;) {
    int rows = 0;
    for (int f = ytop; f <= yf; f++) rows += f == yf ? y_cursor_row(f) + 1 : ed_rows(S.y[f], 4, 4);
    if (rows <= 7 || ytop == yf) break;
    ytop++;
  }
  int row = 1;
  for (int f = ytop; f < 10 && row < 8; f++) {
    char lab[5] = { '\\', 'Y', (char)(f == 9 ? '0' : '1' + f), '=', 0 };
    lcd_text(0, row, lab, 3);
    lcd_char(3, row, '=', S.ysel[f] && S.y[f].n > 0);
    int used;
    if (f == yf && S.y[f].cur < 0) {
      used = ed_draw(S.y[f], 4, row, 4, 8 - row, false);
      if (blink) { if (S.ysel[f] && S.y[f].n) lcd_char(3, row, '=', false); else lcd_char(3, row, '=', true); }
    } else used = ed_draw(S.y[f], 4, row, 4, 8 - row, f == yf);
    row += used > 0 ? used : 1;
  }
}

// ===================================================================== estado del gráfico
static uint8_t gbuf[LCD_H][LCD_W];
static int gmode = G_PLAIN;
static int cx = 47, cy = 31;          // cursor libre
static int tf = 0;                    // función en TRACE
static double tx = 0;
static bool gfree_shown = false;

enum { C_NONE, C_VALUE, C_ZERO, C_MIN, C_MAX, C_INTER, C_DYDX, C_INTEG };
static int calc = C_NONE, cstep = 0, cf1 = 0;
static double cb[2];
static char res_label[24], res_line[40];
static bool vprompt = false;
static Editor ved;
static int zact = 0, zstep = 0, zx0 = 0, zy0 = 0;
static double wprev[7], wsto[7];
static bool has_prev = false, has_sto = false;

static double DX() { return (S.xmax - S.xmin) / 94; }
static double DY() { return (S.ymax - S.ymin) / 62; }
static double col_x(int c) { return r14(S.xmin + c * DX()); }
static double row_y(int r) { return r14(S.ymax - r * DY()); }

static void gset(int x, int y) { if ((unsigned)x < 95 && (unsigned)y < 63) gbuf[y][x] = 1; }
static void gline(int x0, int y0, int x1, int y1) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (int g = 0; g < 3000; g++) {
    gset(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static bool structural(int e) {
  return e == E_SYNTAX || e == E_ARGUMENT || e == E_DATATYPE || e == E_UNDEFINED || e == E_MEMORY || e == E_INVALID;
}

static int clampi(double v) { return v < -2000 ? -2000 : v > 2000 ? 2000 : (int)m_floor(v + 0.5); }

// Dibuja ejes y funciones en gbuf. Devuelve false si hubo un error (ya mostrado).
static bool render(void) {
  if (!(S.xmin < S.xmax) || !(S.ymin < S.ymax)) { show_error(E_WINDOW, -1, 0); return false; }
  if (!graph_dirty) return true;
  for (int y = 0; y < LCD_H; y++) for (int x = 0; x < LCD_W; x++) gbuf[y][x] = 0;
  double dx = DX(), dy = DY();
  int arow = clampi(S.ymax / dy), acol = clampi(-S.xmin / dx);
  bool xaxis = S.ymin <= 0 && S.ymax >= 0, yaxis = S.xmin <= 0 && S.xmax >= 0;
  if (S.grid && S.xscl > 0 && S.yscl > 0 && S.xscl / dx >= 2 && S.yscl / dy >= 2) {
    for (double gx = m_floor(S.xmin / S.xscl) * S.xscl; gx <= S.xmax; gx += S.xscl)
      for (double gy = m_floor(S.ymin / S.yscl) * S.yscl; gy <= S.ymax; gy += S.yscl)
        gset(clampi((gx - S.xmin) / dx), clampi((S.ymax - gy) / dy));
  }
  if (S.axes) {
    if (xaxis) for (int x = 0; x < 95; x++) gset(x, arow);
    if (yaxis) for (int y = 0; y < 63; y++) gset(acol, y);
    if (xaxis && S.xscl > 0 && S.xscl / dx >= 1)
      for (double k = m_floor(S.xmin / S.xscl); k * S.xscl <= S.xmax; k++) {
        int c = clampi((k * S.xscl - S.xmin) / dx);
        gset(c, arow - 1); gset(c, arow + 1);
      }
    if (yaxis && S.yscl > 0 && S.yscl / dy >= 1)
      for (double k = m_floor(S.ymin / S.yscl); k * S.yscl <= S.ymax; k++) {
        int r = clampi((S.ymax - k * S.yscl) / dy);
        gset(acol - 1, r); gset(acol + 1, r);
      }
  }
  int step = (int)S.xres; if (step < 1) step = 1;
  for (int f = 0; f < 10; f++) {
    Editor& e = S.y[f];
    if (!S.ysel[f] || !e.n) continue;
    bool prev = false; int pc = 0, pr = 0;
    for (int c = 0; c < 95; c += step) {
      double y; int pos = 0;
      double old = S.vars[V_X];
      S.vars[V_X] = col_x(c);
      int err = eval_expr(e.t, e.n, &y, &pos);
      S.vars[V_X] = old;
      if (err) {
        if (structural(err)) { graph_dirty = true; show_error(err, SC_YEQ, (f << 8) | (pos & 255)); return false; }
        prev = false; continue;
      }
      int r = clampi((S.ymax - y) / dy);
      if (S.dotmode) gset(c, r);
      else if (prev) gline(pc, pr, c, r);
      else gset(c, r);
      pc = c; pr = r; prev = true;
    }
  }
  graph_dirty = false;
  return true;
}

static int first_fn(int from, int dir) {
  for (int i = 1; i <= 10; i++) {
    int f = ((from + dir * i) % 10 + 10) % 10;
    if (S.ysel[f] && S.y[f].n) return f;
  }
  return (S.ysel[from] && S.y[from].n) ? from : -1;
}

static void clear_result(void) { res_label[0] = 0; res_line[0] = 0; }

void graph_open(int mode) {
  if (!render()) return;
  calc = C_NONE; zact = 0; vprompt = false; clear_result();
  gmode = mode;
  if (mode == G_TRACE) {
    int f = first_fn(9, 1);
    if (f < 0) { gmode = G_FREE; cx = 47; cy = 31; }
    else { tf = f; tx = col_x(47); }
  }
  gfree_shown = false;
  if (mode == G_FREE) { cx = 47; cy = 31; gfree_shown = true; }
  ins_mode = false;
  go(SC_GRAPH);
}

Editor* graph_editor(void) { return vprompt ? &ved : 0; }

static void save_prev(void) {
  double w[7] = { S.xmin, S.xmax, S.xscl, S.ymin, S.ymax, S.yscl, S.xres };
  m_copy(wprev, w, sizeof w); has_prev = true;
}
static void set_window(double x0, double x1, double xs, double y0, double y1, double ys) {
  save_prev();
  S.xmin = r14(x0); S.xmax = r14(x1); S.xscl = r14(xs); S.ymin = r14(y0); S.ymax = r14(y1); S.yscl = r14(ys);
  graph_dirty = true;
}

// ------------------------------------------------------------------ cálculos (CALC)
static int g_f1 = 0, g_f2 = -1;
static double g_eval(double x, int* err) {
  double a, b;
  int e = eval_fn(g_f1, x, &a);
  if (!e && g_f2 >= 0) { e = eval_fn(g_f2, x, &b); a -= b; }
  *err = e;
  return a;
}

// Raíz de g en [lo, hi] (Brent). Devuelve E_NOSIGN si no hay cambio de signo.
static int root(double lo, double hi, double guess, double* out) {
  int e;
  // buscar un subintervalo con cambio de signo, empezando cerca del intento
  const int N = 200;
  double step = (hi - lo) / N;
  int gi = (int)((guess - lo) / step); if (gi < 0) gi = 0; if (gi >= N) gi = N - 1;
  double a = 0, b = 0; bool found = false;
  for (int d = 0; d < N && !found; d++) {
    for (int s = -1; s <= 1 && !found; s += 2) {
      int i = gi + s * d;
      if (i < 0 || i >= N || (d == 0 && s == 1)) continue;
      double x0 = lo + i * step, x1 = i == N - 1 ? hi : lo + (i + 1) * step;
      int e0, e1; double f0 = g_eval(x0, &e0), f1 = g_eval(x1, &e1);
      if (e0 || e1) continue;
      if (f0 == 0) { *out = x0; return E_OK; }
      if (f1 == 0) { *out = x1; return E_OK; }
      if ((f0 < 0) != (f1 < 0)) { a = x0; b = x1; found = true; }
    }
  }
  if (!found) return E_NOSIGN;
  double fa = g_eval(a, &e), fb = g_eval(b, &e), c = b, fc = fb, d = 0, ee = 0;
  for (int it = 0; it < 200; it++) {
    if ((fb > 0) == (fc > 0)) { c = a; fc = fa; d = b - a; ee = d; }
    if (m_fabs(fc) < m_fabs(fb)) { a = b; b = c; c = a; fa = fb; fb = fc; fc = fa; }
    double tol = 2e-15 * m_fabs(b) + 1e-14, xm = (c - b) / 2;
    if (m_fabs(xm) <= tol || fb == 0) break;
    if (m_fabs(ee) >= tol && m_fabs(fa) > m_fabs(fb)) {
      double s = fb / fa, p, q;
      if (a == c) { p = 2 * xm * s; q = 1 - s; }
      else { double qq = fa / fc, r = fb / fc; p = s * (2 * xm * qq * (qq - r) - (b - a) * (r - 1)); q = (qq - 1) * (r - 1) * (s - 1); }
      if (p > 0) q = -q; p = m_fabs(p);
      if (2 * p < (3 * xm * q - m_fabs(tol * q) < m_fabs(ee * q) ? 3 * xm * q - m_fabs(tol * q) : m_fabs(ee * q))) { ee = d; d = p / q; }
      else { d = xm; ee = d; }
    } else { d = xm; ee = d; }
    a = b; fa = fb;
    b += m_fabs(d) > tol ? d : (xm > 0 ? tol : -tol);
    fb = g_eval(b, &e);
    if (e) break;
  }
  *out = b;
  return E_OK;
}

static void set_result(const char* label, double x, double y) {
  s_cpy(res_label, label, sizeof res_label);
  tx = x; S.vars[V_X] = x; S.vars[V_Y] = y;
  res_line[0] = 0;
}

static void calc_prompt(char* out) {
  out[0] = 0;
  switch (calc) {
    case C_ZERO: case C_MIN: case C_MAX:
      s_cpy(out, cstep == 0 ? "Left Bound?" : cstep == 1 ? "Right Bound?" : "Guess?", 20); break;
    case C_INTER: s_cpy(out, cstep == 0 ? "First curve?" : cstep == 1 ? "Second curve?" : "Guess?", 20); break;
    case C_INTEG: s_cpy(out, cstep == 0 ? "Lower Limit?" : "Upper Limit?", 20); break;
  }
}

static void calc_enter(void) {
  int err = 0; double y;
  switch (calc) {
    case C_ZERO: case C_MIN: case C_MAX:
      if (cstep < 2) {
        cb[cstep] = tx;
        if (cstep == 1 && cb[1] < cb[0]) { calc = C_NONE; show_error(E_BOUND, -1, 0); return; }
        cstep++; return;
      }
      {
        double guess = tx, x;
        if (calc == C_ZERO) {
          g_f1 = tf; g_f2 = -1;
          err = root(cb[0], cb[1], guess, &x);
        } else x = minimize(S.y[tf].t, S.y[tf].n, V_X, cb[0], cb[1], calc == C_MAX, &err);
        if (!err) err = eval_fn(tf, x, &y);
        int which = calc; calc = C_NONE;
        if (err) { show_error(err, -1, 0); return; }
        set_result(which == C_ZERO ? "Zero" : which == C_MIN ? "Minimum" : "Maximum", r14(x), r14(y));
      }
      return;
    case C_INTER:
      if (cstep == 0) { cf1 = tf; int f = first_fn(tf, 1); if (f >= 0) tf = f; cstep = 1; return; }
      if (cstep == 1) {
        if (tf == cf1) { calc = C_NONE; show_error(E_INVALID, -1, 0); return; }
        cstep = 2; return;
      }
      {
        double x; g_f1 = cf1; g_f2 = tf;
        err = root(S.xmin, S.xmax, tx, &x);
        if (!err) err = eval_fn(cf1, x, &y);
        calc = C_NONE;
        if (err) { show_error(err, -1, 0); return; }
        set_result("Intersection", r14(x), r14(y));
      }
      return;
    case C_DYDX: {
      double a, b;
      err = eval_fn(tf, tx + 1e-3, &a);
      if (!err) err = eval_fn(tf, tx - 1e-3, &b);
      calc = C_NONE;
      if (err) { show_error(err, -1, 0); return; }
      char v[16]; fmt_num(r14((a - b) / 2e-3), v, 12);
      s_cpy(res_line, "dy/dx=", sizeof res_line);
      s_cpy(res_line + 6, v, sizeof res_line - 6);
      S.ans = r14((a - b) / 2e-3);
      return;
    }
    case C_INTEG:
      if (cstep == 0) { cb[0] = tx; cstep = 1; return; }
      {
        cb[1] = tx;
        double lo = cb[0] < cb[1] ? cb[0] : cb[1], hi = cb[0] < cb[1] ? cb[1] : cb[0];
        double r = integrate(S.y[tf].t, S.y[tf].n, V_X, cb[0], cb[1], &err);
        calc = C_NONE;
        if (err) { show_error(err, -1, 0); return; }
        // sombrear el área entre la curva y el eje x
        double dx = DX(), dy = DY();
        int arow = clampi(S.ymax / dy); if (arow < 0) arow = 0; if (arow > 62) arow = 62;
        for (int c = clampi((lo - S.xmin) / dx); c <= clampi((hi - S.xmin) / dx); c++) {
          if (c < 0 || c > 94) continue;
          double yy; if (eval_fn(tf, col_x(c), &yy)) continue;
          int r = clampi((S.ymax - yy) / dy); if (r < 0) r = 0; if (r > 62) r = 62;
          for (int k = r < arow ? r : arow; k <= (r < arow ? arow : r); k++) gset(c, k);
        }
        char v[16]; fmt_num(r14(r), v, 12);
        s_cpy(res_line, "\x90" "f(x)dx=", sizeof res_line);
        s_cpy(res_line + 8, v, sizeof res_line - 8);
        S.ans = r14(r);
      }
      return;
  }
}

void graph_calc(int what) {
  if (!render()) return;
  int f = first_fn(9, 1);
  zact = 0; clear_result(); vprompt = false;
  if (f < 0) { gmode = G_FREE; cx = 47; cy = 31; gfree_shown = true; calc = C_NONE; go(SC_GRAPH); return; }
  if (gmode != G_TRACE || !(S.ysel[tf] && S.y[tf].n)) { tf = f; tx = col_x(47); }
  gmode = G_TRACE;
  calc = what; cstep = 0;
  if (what == C_VALUE) { vprompt = true; ed_clear(ved); }
  go(SC_GRAPH);
}

// ------------------------------------------------------------------ ZOOM
static void zoom_fit(void) {
  double lo = 1e300, hi = -1e300;
  for (int f = 0; f < 10; f++) {
    if (!S.ysel[f] || !S.y[f].n) continue;
    for (int c = 0; c < 95; c++) { double y; if (!eval_fn(f, col_x(c), &y)) { if (y < lo) lo = y; if (y > hi) hi = y; } }
  }
  if (lo > hi) return;
  if (lo == hi) { lo -= 1; hi += 1; }
  set_window(S.xmin, S.xmax, S.xscl, lo, hi, S.yscl);
}

void graph_zoom(int what) {
  switch (what) {
    case 1: case 2: case 3: case 8:                  // usan el cursor
      if (!render()) return;
      zact = what; zstep = 0; gmode = G_FREE; gfree_shown = true; cx = 47; cy = 31;
      calc = C_NONE; vprompt = false; clear_result();
      go(SC_GRAPH);
      return;
    case 4: set_window(-4.7, 4.7, 1, -3.1, 3.1, 1); break;
    case 5: {
      double dx = DX(), dy = DY();
      if (dx > dy) { double c = (S.ymin + S.ymax) / 2, h = dx * 31; set_window(S.xmin, S.xmax, S.xscl, c - h, c + h, S.yscl); }
      else { double c = (S.xmin + S.xmax) / 2, h = dy * 47; set_window(c - h, c + h, S.xscl, S.ymin, S.ymax, S.yscl); }
      break;
    }
    case 6: set_window(-10, 10, 1, -10, 10, 1); S.xres = 1; break;
    case 7:
      if (S.degree) set_window(-352.5, 352.5, 90, -4, 4, 1);
      else set_window(-6.1522856132799, 6.1522856132799, 1.5707963267949, -4, 4, 1);
      break;
    case 10: zoom_fit(); break;
    case 11:
      if (has_prev) { double w[7]; m_copy(w, wprev, sizeof w); set_window(w[0], w[1], w[2], w[3], w[4], w[5]); S.xres = w[6]; }
      break;
    case 12: { double w[7] = { S.xmin, S.xmax, S.xscl, S.ymin, S.ymax, S.yscl, S.xres }; m_copy(wsto, w, sizeof w); has_sto = true; break; }
    case 13: if (has_sto) { set_window(wsto[0], wsto[1], wsto[2], wsto[3], wsto[4], wsto[5]); S.xres = wsto[6]; } break;
    case 14: form_open(FORM_FACTORS); return;
  }
  graph_open(G_PLAIN);
}

static void zoom_enter(void) {
  double x = col_x(cx), y = row_y(cy);
  if (zact == 1) {                                   // ZBox
    if (zstep == 0) { zx0 = cx; zy0 = cy; zstep = 1; return; }
    if (cx == zx0 || cy == zy0) return;
    double xa = col_x(zx0 < cx ? zx0 : cx), xb = col_x(zx0 < cx ? cx : zx0);
    double ya = row_y(zy0 > cy ? zy0 : cy), yb = row_y(zy0 > cy ? cy : zy0);
    set_window(xa, xb, S.xscl, ya, yb, S.yscl);
  } else if (zact == 2 || zact == 3) {
    double w = (S.xmax - S.xmin) / 2, h = (S.ymax - S.ymin) / 2;
    if (zact == 2) { w /= S.xfact; h /= S.yfact; } else { w *= S.xfact; h *= S.yfact; }
    set_window(x - w, x + w, S.xscl, y - h, y + h, S.yscl);
  } else {                                           // ZInteger
    double ix = m_floor(x + 0.5), iy = m_floor(y + 0.5);
    set_window(ix - 47, ix + 47, 10, iy - 31, iy + 31, 10);
  }
  zact = 0;
  graph_open(G_PLAIN);
}

// ------------------------------------------------------------------ teclas del gráfico
static void trace_move(int dir) {
  tx = r14(tx + dir * DX());
  double dx = DX();
  if (tx > S.xmax + dx / 2 || tx < S.xmin - dx / 2) {   // se salió: desplazar la ventana
    double shift = dir * 47 * dx;
    S.xmin = r14(S.xmin + shift); S.xmax = r14(S.xmax + shift);
    graph_dirty = true; render();
  }
}

static bool value_entered(double x) {
  if (calc == C_VALUE) {
    if (x < S.xmin || x > S.xmax) { calc = C_NONE; vprompt = false; show_error(E_INVALID, -1, 0); return false; }
    double y; int err = eval_fn(tf, x, &y);
    tx = x; calc = C_NONE;
    if (!err) { S.vars[V_X] = x; S.vars[V_Y] = y; }
    return true;
  }
  tx = x;
  if (calc != C_NONE) calc_enter();
  return true;
}

void graph_key(int k, int mod) {
  if (vprompt) {
    if (mod == MOD_NONE) {
      if (k == K_ENTER) {
        double x; int pos = 0;
        if (!ved.n) { vprompt = calc == C_VALUE; return; }
        int e = eval_expr(ved.t, ved.n, &x, &pos);
        vprompt = false;
        if (e) { calc = C_NONE; show_error(e, -1, 0); return; }
        value_entered(r14(x));
        return;
      }
      if (k == K_CLEAR) { if (ved.n) ed_clear(ved); else { vprompt = false; if (calc == C_VALUE) calc = C_NONE; } return; }
      if (k == K_DEL) { ed_back(ved); return; }
      if (k == K_LEFT) { if (ved.cur > 0) ved.cur--; return; }
      if (k == K_RIGHT) { if (ved.cur < ved.n) ved.cur++; return; }
    }
    int t = key_token(k, mod);
    if (t >= 0 && t != T_STO) { ed_put(ved, (uint8_t)t); return; }
    if (mod == MOD_2ND && k == K_MODE) { vprompt = false; calc = C_NONE; go_home(); return; }
    return;
  }
  if (zact) {
    if (mod == MOD_NONE) {
      switch (k) {
        case K_LEFT: if (cx > 0) cx--; return;
        case K_RIGHT: if (cx < 94) cx++; return;
        case K_UP: if (cy > 0) cy--; return;
        case K_DOWN: if (cy < 62) cy++; return;
        case K_ENTER: zoom_enter(); return;
        case K_CLEAR: zact = 0; gmode = G_PLAIN; return;
      }
    }
    zact = 0;
    nav_key(k, mod);
    return;
  }
  if (gmode == G_TRACE) {
    if (mod == MOD_NONE) {
      switch (k) {
        case K_LEFT: clear_result(); trace_move(-1); return;
        case K_RIGHT: clear_result(); trace_move(1); return;
        case K_UP: case K_DOWN: {
          if (calc == C_DYDX || calc == C_NONE || calc == C_INTER || calc == C_ZERO || calc == C_MIN || calc == C_MAX || calc == C_INTEG) {
            int f = first_fn(tf, k == K_UP ? -1 : 1); if (f >= 0) tf = f;
          }
          clear_result();
          return;
        }
        case K_ENTER:
          if (calc != C_NONE) { clear_result(); calc_enter(); return; }
          {                                          // centrar la ventana en el cursor
            double y; if (eval_fn(tf, tx, &y)) y = (S.ymin + S.ymax) / 2;
            double w = (S.xmax - S.xmin) / 2, h = (S.ymax - S.ymin) / 2;
            set_window(tx - w, tx + w, S.xscl, y - h, y + h, S.yscl);
            render();
          }
          return;
        case K_CLEAR:
          if (calc != C_NONE) { calc = C_NONE; clear_result(); return; }
          gmode = G_PLAIN; clear_result(); return;
      }
    }
    int t = key_token(k, mod);                       // escribir un número: ir a ese X
    if (t >= 0 && (t <= T_DOT || t == T_NEG || t == T_PI || t == T_E || t == T_LP)) {
      vprompt = true; ed_clear(ved); ed_put(ved, (uint8_t)t); return;
    }
    if (calc != C_NONE) { calc = C_NONE; clear_result(); }
    nav_key(k, mod);
    return;
  }
  if (mod == MOD_NONE) {
    switch (k) {
      case K_LEFT: case K_RIGHT: case K_UP: case K_DOWN:
        if (gmode != G_FREE || !gfree_shown) { gmode = G_FREE; gfree_shown = true; cx = 47; cy = 31; return; }
        if (k == K_LEFT && cx > 0) cx--;
        if (k == K_RIGHT && cx < 94) cx++;
        if (k == K_UP && cy > 0) cy--;
        if (k == K_DOWN && cy < 62) cy++;
        return;
      case K_ENTER: return;
      case K_CLEAR: go_home(); return;
    }
  }
  nav_key(k, mod);
}

// ------------------------------------------------------------------ dibujo
static void small_box(int x, int y, const char* s) {
  int n = s_len(s);
  lcd_fill(x, y - 1, n * 4 + 1, 7, 0);
  lcd_small(x, y, s, n);
}

static void coords(double x, double y, bool has_y) {
  char a[16], b[16], line[40];
  fmt_num(x, a, 9);
  int o = 0; line[o++] = 'X'; line[o++] = '=';
  for (int i = 0; a[i]; i++) line[o++] = a[i];
  line[o] = 0;
  small_box(0, 57, line);
  if (has_y) {
    fmt_num(y, b, 9);
    o = 0; line[o++] = 'Y'; line[o++] = '=';
    for (int i = 0; b[i]; i++) line[o++] = b[i];
    line[o] = 0;
    small_box(48, 57, line);
  }
}

static void xor_px(int x, int y) { if ((unsigned)x < 95 && (unsigned)y < 63) lcd_set(x, y, !lcd_get(x, y)); }

void graph_draw(void) {
  if (graph_dirty && !render()) return;
  lcd_clear();
  for (int y = 0; y < LCD_H; y++) for (int x = 0; x < LCD_W; x++) lcd_px[y][x] = gbuf[y][x];
  if (gmode == G_TRACE) {
    double y; int err = eval_fn(tf, tx, &y);
    int c = clampi((tx - S.xmin) / DX());
    if (!err) {
      int r = clampi((S.ymax - y) / DY());
      for (int i = -2; i <= 2; i++) { xor_px(c + i, r - 2); xor_px(c + i, r + 2); }
      for (int i = -1; i <= 1; i++) { xor_px(c - 2, r + i); xor_px(c + 2, r + i); }
    }
    if (S.expr) {
      char line[64] = { 'Y', (char)(tf == 9 ? '0' : '1' + tf), '=' };
      int n = ed_text(S.y[tf], line + 3, sizeof line - 3, 0) + 3;
      if (n > 24) n = 24;
      line[n] = 0;
      small_box(0, 1, line);
    }
    // marcas de los límites ya elegidos
    if ((calc == C_ZERO || calc == C_MIN || calc == C_MAX || calc == C_INTEG) && cstep > 0) {
      for (int i = 0; i < cstep && i < 2; i++) {      // ▸ límite izquierdo, ◂ límite derecho
        int bc = clampi((cb[i] - S.xmin) / DX()), d = i == 0 ? 1 : -1;
        for (int k = 0; k < 3; k++) for (int j = -(2 - k); j <= 2 - k; j++) lcd_set(bc + d * k, 10 + j, 1);
      }
    }
    char prompt[24]; calc_prompt(prompt);
    if (vprompt) {
      lcd_fill(0, 50, 96, 14, 0);
      lcd_small_str(0, 57, "X=");
      char buf[64]; int n = ed_text(ved, buf, sizeof buf, 0);
      lcd_small(8, 57, buf, n > 21 ? 21 : n);
      if (blink) lcd_fill(8 + (n > 21 ? 21 : n) * 4, 57, 3, 5, 1);
    } else if (res_line[0]) {
      small_box(0, 57, res_line);
    } else {
      if (prompt[0]) small_box(0, 50, prompt);
      if (res_label[0]) small_box(0, 50, res_label);
      if (S.coord || res_label[0]) coords(tx, err ? 0 : y, !err);
    }
  } else if (gmode == G_FREE && gfree_shown) {
    for (int i = -2; i <= 2; i++) { if (i) { xor_px(cx + i, cy); xor_px(cx, cy + i); } }
    if (blink) xor_px(cx, cy);
    if (zact == 1 && zstep == 1) {
      int x0 = zx0 < cx ? zx0 : cx, x1 = zx0 < cx ? cx : zx0, y0 = zy0 < cy ? zy0 : cy, y1 = zy0 < cy ? cy : zy0;
      for (int x = x0; x <= x1; x++) { xor_px(x, y0); if (y1 != y0) xor_px(x, y1); }
      for (int y = y0 + 1; y < y1; y++) { xor_px(x0, y); if (x1 != x0) xor_px(x1, y); }
    }
    if (S.coord) coords(col_x(cx), row_y(cy), true);
  }
}

void graph_reset(void) {
  yf = 0; gmode = G_PLAIN; calc = C_NONE; zact = 0; vprompt = false; has_prev = has_sto = false;
  clear_result(); graph_dirty = true;
}

// ===================================================================== TABLE
static int ttop = 0, trow = 0, tcol = 0, tfirst = 0;

void table_open(void) { ttop = 0; trow = 0; tcol = 0; tfirst = 0; go(SC_TABLE); }

static int table_fns(int* list) {
  int n = 0;
  for (int f = 0; f < 10; f++) if (S.ysel[f] && S.y[f].n) list[n++] = f;
  return n;
}

void table_key(int k, int mod) {
  int list[10], nf = table_fns(list);
  if (mod == MOD_NONE) {
    switch (k) {
      case K_UP: if (trow > 0) trow--; else ttop--; return;
      case K_DOWN: if (trow < 6) trow++; else ttop++; return;
      case K_LEFT:
        if (tcol == 1 && tfirst > 0) tfirst--;
        else if (tcol > 0) tcol--;
        return;
      case K_RIGHT:
        if (tcol < 2 && tfirst + tcol < nf) tcol++;
        else if (tcol == 2 && tfirst + 2 < nf) tfirst++;
        return;
      case K_CLEAR: go(SC_HOME); return;
      case K_ENTER: return;
    }
  }
  nav_key(k, mod);
}

static void tbl_text(int x, int y, const char* s, int n, bool inv) {
  for (int i = 0; i < n; i++) {
    lcd_glyph_px(x + i * 6, y, (uint8_t)s[i], 1, 7, inv);
    if (lcd_log_n < (int)sizeof lcd_log - 2) lcd_log[lcd_log_n++] = s[i];
  }
  if (lcd_log_n < (int)sizeof lcd_log - 1) { lcd_log[lcd_log_n++] = '|'; lcd_log[lcd_log_n] = 0; }
}

static int cell(int f, double x, char* out) {
  double y;
  if (f < 0) return fmt_num(x, out, 5);
  if (eval_fn(f, x, &y)) { s_cpy(out, "ERROR", 8); return 5; }
  return fmt_num(y, out, 5);
}

void table_draw(void) {
  lcd_clear();
  int list[10], nf = table_fns(list);
  int cols = nf - tfirst + 1; if (cols > 3) cols = 3;
  if (tcol >= cols) tcol = cols - 1;
  for (int c = 0; c < 3; c++) {
    int x0 = c * 32;
    if (c > 0) for (int y = 0; y < 57; y++) lcd_set(x0 - 1, y, 1);
    if (c >= cols) continue;
    char h[4];
    if (c == 0) { h[0] = 'X'; h[1] = 0; }
    else { int f = list[tfirst + c - 1]; h[0] = 'Y'; h[1] = (char)(f == 9 ? '0' : '1' + f); h[2] = 0; }
    int hn = s_len(h);
    tbl_text(x0 + (31 - hn * 6) / 2, 0, h, hn, false);
    for (int r = 0; r < 7; r++) {
      double x = r14(S.tblstart + (ttop + r) * S.dtbl);
      char v[16]; int n = cell(c == 0 ? -1 : list[tfirst + c - 1], x, v);
      bool sel = r == trow && c == tcol;
      if (sel) lcd_fill(x0, 8 + r * 7, 31, 7, 1);
      tbl_text(x0 + 1 + (5 - n) * 6, 8 + r * 7, v, n, sel);
    }
  }
  for (int x = 0; x < 96; x++) lcd_set(x, 7, 1);
  // línea de abajo: valor completo de la celda elegida
  double x = r14(S.tblstart + (ttop + trow) * S.dtbl);
  char line[40]; int o = 0;
  if (tcol == 0) { line[o++] = 'X'; line[o++] = '='; char v[20]; int n = fmt_num(x, v, 14); for (int i = 0; i < n; i++) line[o++] = v[i]; }
  else {
    int f = list[tfirst + tcol - 1];
    line[o++] = 'Y'; line[o++] = (char)(f == 9 ? '0' : '1' + f); line[o++] = '=';
    double y; char v[20];
    if (eval_fn(f, x, &y)) { s_cpy(v, "ERROR", 8); } else fmt_num(y, v, 13);
    for (int i = 0; v[i]; i++) line[o++] = v[i];
  }
  line[o] = 0;
  tbl_text(0, 57, line, o > 16 ? 16 : o, false);
}
