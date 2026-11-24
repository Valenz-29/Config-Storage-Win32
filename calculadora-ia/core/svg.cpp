// svg.cpp - Dibuja un subconjunto de SVG (lo que la IA usa para mostrar dibujos).
// Soporta: svg (viewBox), g, line, rect, circle, ellipse, polyline, polygon, path
// (M L H V C S Q T A Z, absolutos y relativos), text/tspan, colores, stroke-width,
// fill/stroke en atributos o style, y flechas con marker-end.
#include "svg.h"
#include "gfx.h"
#include "hal.h"

// ------------------------------------------------------------------ utilidades
static bool sp(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ','; }
static bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static bool eq_n(const char* a, int n, const char* b) {
  int i = 0;
  for (; i < n && b[i]; i++) if (lower(a[i]) != lower(b[i])) return false;
  return i == n && !b[i];
}

// número tipo SVG: -1.5e2, .5, 3px
static bool num(const char*& p, const char* end, double* out) {
  while (p < end && sp(*p)) p++;
  if (p >= end) return false;
  const char* s = p;
  double sign = 1;
  if (*p == '-' || *p == '+') { if (*p == '-') sign = -1; p++; }
  double v = 0; bool any = false;
  while (p < end && *p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; any = true; }
  if (p < end && *p == '.') {
    p++; double f = 0.1;
    while (p < end && *p >= '0' && *p <= '9') { v += (*p - '0') * f; f /= 10; p++; any = true; }
  }
  if (!any) { p = s; return false; }
  if (p < end && (*p == 'e' || *p == 'E') && p + 1 < end && ((p[1] >= '0' && p[1] <= '9') || p[1] == '-' || p[1] == '+')) {
    p++; int es = 1; if (*p == '-' || *p == '+') { if (*p == '-') es = -1; p++; }
    int e = 0; while (p < end && *p >= '0' && *p <= '9') { e = e * 10 + (*p - '0'); p++; }
    double m = 1; for (int i = 0; i < e && i < 30; i++) m *= 10;
    v = es > 0 ? v * m : v / m;
  }
  while (p < end && is_alpha(*p) && *p != 'e' && *p != 'E') {   // unidades (px, pt): se ignoran
    if (lower(*p) == 'p' || lower(*p) == 'x' || lower(*p) == 't') p++; else break;
  }
  *out = sign * v;
  return true;
}

static double num_or(const char* s, int n, double def) { const char* p = s; double v; return num(p, s + n, &v) ? v : def; }

// ------------------------------------------------------------------ colores
struct Named { const char* n; uint8_t r, g, b; };
static const Named COLORS[] = {
  { "black", 0, 0, 0 }, { "white", 255, 255, 255 }, { "red", 230, 40, 40 }, { "green", 0, 160, 60 },
  { "blue", 40, 90, 230 }, { "yellow", 245, 210, 0 }, { "orange", 255, 140, 0 }, { "purple", 140, 60, 190 },
  { "gray", 128, 128, 128 }, { "grey", 128, 128, 128 }, { "lightgray", 200, 200, 200 }, { "lightgrey", 200, 200, 200 },
  { "darkgray", 90, 90, 90 }, { "darkgrey", 90, 90, 90 }, { "brown", 140, 80, 30 }, { "pink", 255, 150, 190 },
  { "cyan", 0, 200, 220 }, { "magenta", 220, 0, 200 }, { "lime", 80, 220, 60 }, { "navy", 20, 30, 120 },
  { "teal", 0, 128, 128 }, { "maroon", 128, 0, 0 }, { "olive", 128, 128, 0 }, { "silver", 192, 192, 192 },
  { "gold", 230, 180, 0 }, { "lightblue", 150, 200, 240 }, { "skyblue", 120, 190, 235 }, { "darkblue", 0, 0, 140 },
  { "darkgreen", 0, 100, 0 }, { "darkred", 140, 0, 0 }, { "lightgreen", 150, 230, 150 }, { "violet", 200, 120, 230 },
};
#define C_NONE -1
#define C_UNSET -2

static int hexv(char c) { c = (char)lower(c); return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }

static int color(const char* s, int n) {
  while (n && sp(*s)) { s++; n--; }
  while (n && sp(s[n - 1])) n--;
  if (!n) return C_UNSET;
  if (eq_n(s, n, "none") || eq_n(s, n, "transparent")) return C_NONE;
  if (eq_n(s, n, "currentcolor")) return 0;
  if (s[0] == '#') {
    if (n == 4) { int r = hexv(s[1]), g = hexv(s[2]), b = hexv(s[3]); if (r >= 0 && g >= 0 && b >= 0) return rgb(r * 17, g * 17, b * 17); }
    if (n >= 7) {
      int v[6]; for (int i = 0; i < 6; i++) v[i] = hexv(s[1 + i]);
      if (v[0] >= 0 && v[1] >= 0 && v[2] >= 0 && v[3] >= 0 && v[4] >= 0 && v[5] >= 0) return rgb(v[0] * 16 + v[1], v[2] * 16 + v[3], v[4] * 16 + v[5]);
    }
    return 0;
  }
  if (n > 4 && lower(s[0]) == 'r' && lower(s[1]) == 'g' && lower(s[2]) == 'b') {
    const char* p = s + 3; while (p < s + n && *p != '(') p++; p++;
    double c[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++) num(p, s + n, &c[i]);
    return rgb((int)c[0] & 255, (int)c[1] & 255, (int)c[2] & 255);
  }
  for (unsigned i = 0; i < sizeof COLORS / sizeof COLORS[0]; i++)
    if (eq_n(s, n, COLORS[i].n)) return rgb(COLORS[i].r, COLORS[i].g, COLORS[i].b);
  return 0;
}

// ------------------------------------------------------------------ lienzo y transformación
static int cx0, cy0, cw, ch;          // rectángulo destino en la pantalla
static double vbx, vby, scale, ox, oy;
static bool draw_text;

static void pset(int x, int y, uint16_t c) {
  if (x < cx0 || y < cy0 || x >= cx0 + cw || y >= cy0 + ch || x < 0 || y < 0 || x >= SCR_W || y >= SCR_H) return;
  g_fb[y * SCR_W + x] = c;
}
static double TX(double x) { return ox + (x - vbx) * scale; }
static double TY(double y) { return oy + (y - vby) * scale; }
static int R(double v) { return v < -30000 ? -30000 : v > 30000 ? 30000 : (int)(v < 0 ? v - 0.5 : v + 0.5); }

static void brush(int x, int y, int t, uint16_t c) {
  int a = -(t - 1) / 2;
  for (int j = 0; j < t; j++) for (int i = 0; i < t; i++) pset(x + a + i, y + a + j, c);
}
static void line_px(int x0, int y0, int x1, int y1, int t, uint16_t c) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (int guard = 0; guard < 4000; guard++) {
    brush(x0, y0, t, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// polígono relleno (par-impar) en coordenadas de pantalla
#define MAXPTS 400
static double PX[MAXPTS], PY[MAXPTS];
static int npts;
static void fill_poly(const double* xs, const double* ys, int n, uint16_t c) {
  if (n < 3) return;
  double ymin = ys[0], ymax = ys[0];
  for (int i = 1; i < n; i++) { if (ys[i] < ymin) ymin = ys[i]; if (ys[i] > ymax) ymax = ys[i]; }
  int y0 = R(ymin), y1 = R(ymax);
  if (y0 < cy0) y0 = cy0; if (y1 > cy0 + ch - 1) y1 = cy0 + ch - 1;
  for (int y = y0; y <= y1; y++) {
    double yc = y + 0.5, xs_[64]; int k = 0;
    for (int i = 0, j = n - 1; i < n && k < 64; j = i++) {
      if ((ys[i] > yc) != (ys[j] > yc)) xs_[k++] = xs[i] + (yc - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]);
    }
    for (int a = 1; a < k; a++) for (int b = a; b > 0 && xs_[b - 1] > xs_[b]; b--) { double t = xs_[b]; xs_[b] = xs_[b - 1]; xs_[b - 1] = t; }
    for (int a = 0; a + 1 < k; a += 2) for (int x = R(xs_[a]); x < R(xs_[a + 1]); x++) pset(x, y, c);
  }
}

static void arrow_head(double x0, double y0, double x1, double y1, int t, uint16_t c) {   // en pantalla
  double dx = x1 - x0, dy = y1 - y0, len = dx * dx + dy * dy;
  if (len < 0.01) return;
  double l = m_sqrt(len), ux = dx / l, uy = dy / l, s = 5 + 2 * t;
  double xs[3] = { x1, x1 - ux * s - uy * s * 0.5, x1 - ux * s + uy * s * 0.5 };
  double ys[3] = { y1, y1 - uy * s + ux * s * 0.5, y1 - uy * s - ux * s * 0.5 };
  fill_poly(xs, ys, 3, c);
}

// ------------------------------------------------------------------ estilo heredado (g)
struct Style { int fill, stroke; double sw; bool arrow; };
static Style stk[10]; static int depth;

// dibuja la figura acumulada en PX/PY (coordenadas de pantalla)
static void paint(const Style& st, bool closed, bool fillable) {
  if (npts < 1) return;
  int t = R(st.sw * scale); if (t < 1) t = 1; if (t > 6) t = 6;
  if (fillable && st.fill != C_NONE && closed && npts >= 3) fill_poly(PX, PY, npts, (uint16_t)(st.fill == C_UNSET ? 0 : st.fill));
  if (st.stroke >= 0) {
    for (int i = 1; i < npts; i++) line_px(R(PX[i - 1]), R(PY[i - 1]), R(PX[i]), R(PY[i]), t, (uint16_t)st.stroke);
    if (closed && npts > 2) line_px(R(PX[npts - 1]), R(PY[npts - 1]), R(PX[0]), R(PY[0]), t, (uint16_t)st.stroke);
    if (st.arrow && npts >= 2 && !closed) arrow_head(PX[npts - 2], PY[npts - 2], PX[npts - 1], PY[npts - 1], t, (uint16_t)st.stroke);
  }
}
static void add_pt(double x, double y) { if (npts < MAXPTS) { PX[npts] = TX(x); PY[npts] = TY(y); npts++; } }

// ------------------------------------------------------------------ atributos
struct Attrs { const char* v[24]; int vn[24]; const char* k[24]; int kn[24]; int n; };
static bool attr(const Attrs& a, const char* name, const char** v, int* n) {
  for (int i = 0; i < a.n; i++) if (eq_n(a.k[i], a.kn[i], name)) { *v = a.v[i]; *n = a.vn[i]; return true; }
  return false;
}
static double anum(const Attrs& a, const char* name, double def) { const char* v; int n; return attr(a, name, &v, &n) ? num_or(v, n, def) : def; }

// estilo: atributos + style="..." sobre el heredado
static Style style_of(const Attrs& a, Style base) {
  const char* v; int n;
  if (attr(a, "fill", &v, &n)) { int c = color(v, n); if (c != C_UNSET) base.fill = c; }
  if (attr(a, "stroke", &v, &n)) { int c = color(v, n); if (c != C_UNSET) base.stroke = c; }
  if (attr(a, "stroke-width", &v, &n)) base.sw = num_or(v, n, base.sw);
  if (attr(a, "marker-end", &v, &n)) base.arrow = !eq_n(v, n, "none");
  if (attr(a, "style", &v, &n)) {
    const char* p = v; const char* e = v + n;
    while (p < e) {
      const char* k = p; while (p < e && *p != ':') p++;
      int kn = (int)(p - k); while (kn && sp(k[kn - 1])) kn--; while (kn && sp(*k)) { k++; kn--; }
      if (p >= e) break;
      p++; const char* vv = p; while (p < e && *p != ';') p++;
      int vl = (int)(p - vv);
      if (eq_n(k, kn, "fill")) { int c = color(vv, vl); if (c != C_UNSET) base.fill = c; }
      else if (eq_n(k, kn, "stroke")) { int c = color(vv, vl); if (c != C_UNSET) base.stroke = c; }
      else if (eq_n(k, kn, "stroke-width")) base.sw = num_or(vv, vl, base.sw);
      else if (eq_n(k, kn, "marker-end")) base.arrow = !eq_n(vv, vl, "none");
      if (p < e) p++;
    }
  }
  return base;
}

// ------------------------------------------------------------------ path
static void path(const char* d, int n, const Style& st) {
  const char* p = d; const char* e = d + n;
  double x = 0, y = 0, sx = 0, sy = 0, lcx = 0, lcy = 0;   // punto actual, inicio, último control
  char cmd = 0, prev = 0;
  npts = 0;
  bool closed = false;
  while (p < e) {
    while (p < e && sp(*p)) p++;
    if (p >= e) break;
    if (is_alpha(*p)) { cmd = *p++; }
    else if (!cmd) break;
    bool rel = cmd >= 'a';
    char C = (char)(rel ? cmd - 32 : cmd);
    double a[7];
    if (C == 'Z') {
      if (npts) { paint(st, true, true); npts = 0; closed = false; }
      x = sx; y = sy; prev = 'Z'; cmd = 0; continue;
    }
    int need = C == 'H' || C == 'V' ? 1 : C == 'M' || C == 'L' || C == 'T' ? 2 : C == 'S' || C == 'Q' ? 4 : C == 'C' ? 6 : C == 'A' ? 7 : 0;
    if (!need) break;
    bool ok = true;
    for (int i = 0; i < need; i++) if (!num(p, e, &a[i])) { ok = false; break; }
    if (!ok) break;
    double bx = rel ? x : 0, by = rel ? y : 0;
    switch (C) {
      case 'M':
        if (npts > 1) paint(st, false, true);
        npts = 0; x = bx + a[0]; y = by + a[1]; sx = x; sy = y; add_pt(x, y);
        cmd = rel ? 'l' : 'L';                 // pares siguientes = líneas
        break;
      case 'L': x = bx + a[0]; y = by + a[1]; add_pt(x, y); break;
      case 'H': x = bx + a[0]; add_pt(x, y); break;
      case 'V': y = (rel ? y : 0) + a[0]; add_pt(x, y); break;
      case 'C': case 'S': case 'Q': case 'T': {
        double c1x, c1y, c2x, c2y, ex, ey;
        bool cubic = C == 'C' || C == 'S';
        if (C == 'C') { c1x = bx + a[0]; c1y = by + a[1]; c2x = bx + a[2]; c2y = by + a[3]; ex = bx + a[4]; ey = by + a[5]; }
        else if (C == 'S') {
          bool refl = prev == 'C' || prev == 'S';
          c1x = refl ? 2 * x - lcx : x; c1y = refl ? 2 * y - lcy : y;
          c2x = bx + a[0]; c2y = by + a[1]; ex = bx + a[2]; ey = by + a[3];
        } else if (C == 'Q') { c1x = bx + a[0]; c1y = by + a[1]; c2x = c1x; c2y = c1y; ex = bx + a[2]; ey = by + a[3]; }
        else {
          bool refl = prev == 'Q' || prev == 'T';
          c1x = refl ? 2 * x - lcx : x; c1y = refl ? 2 * y - lcy : y; c2x = c1x; c2y = c1y; ex = bx + a[0]; ey = by + a[1];
        }
        for (int i = 1; i <= 16; i++) {
          double t = i / 16.0, u = 1 - t, px_, py_;
          if (cubic) { px_ = u * u * u * x + 3 * u * u * t * c1x + 3 * u * t * t * c2x + t * t * t * ex; py_ = u * u * u * y + 3 * u * u * t * c1y + 3 * u * t * t * c2y + t * t * t * ey; }
          else { px_ = u * u * x + 2 * u * t * c1x + t * t * ex; py_ = u * u * y + 2 * u * t * c1y + t * t * ey; }
          add_pt(px_, py_);
        }
        lcx = c2x; lcy = c2y; x = ex; y = ey;
        break;
      }
      case 'A': {                               // arco: se aproxima con su centro real
        double rx = a[0] < 0 ? -a[0] : a[0], ry = a[1] < 0 ? -a[1] : a[1], ex = bx + a[5], ey = by + a[6];
        int large = a[3] != 0, sweep = a[4] != 0;
        double dx2 = (x - ex) / 2, dy2 = (y - ey) / 2;
        if (rx < 1e-9 || ry < 1e-9) { x = ex; y = ey; add_pt(x, y); break; }
        double lam = dx2 * dx2 / (rx * rx) + dy2 * dy2 / (ry * ry);
        if (lam > 1) { double s = m_sqrt(lam); rx *= s; ry *= s; }
        double num_ = rx * rx * ry * ry - rx * rx * dy2 * dy2 - ry * ry * dx2 * dx2, den = rx * rx * dy2 * dy2 + ry * ry * dx2 * dx2;
        double co = den > 0 && num_ > 0 ? m_sqrt(num_ / den) : 0;
        if (large == sweep) co = -co;
        double cxp = co * rx * dy2 / ry, cyp = -co * ry * dx2 / rx;
        double ccx = cxp + (x + ex) / 2, ccy = cyp + (y + ey) / 2;
        double t1 = m_atan((y - ccy) / ry / ((x - ccx) / rx == 0 ? 1e-12 : (x - ccx) / rx));
        if ((x - ccx) / rx < 0) t1 += 3.14159265358979;
        double t2 = m_atan((ey - ccy) / ry / ((ex - ccx) / rx == 0 ? 1e-12 : (ex - ccx) / rx));
        if ((ex - ccx) / rx < 0) t2 += 3.14159265358979;
        double dt = t2 - t1;
        while (sweep && dt < 0) dt += 6.28318530717959;
        while (!sweep && dt > 0) dt -= 6.28318530717959;
        for (int i = 1; i <= 20; i++) { double t = t1 + dt * i / 20; add_pt(ccx + rx * m_cos(t), ccy + ry * m_sin(t)); }
        x = ex; y = ey;
        break;
      }
    }
    prev = C;
  }
  (void)closed;
  if (npts > 1) paint(st, false, true);
  npts = 0;
}

// ------------------------------------------------------------------ texto
static int decode_entities(const char* s, int n, char* out, int cap) {
  int o = 0;
  for (int i = 0; i < n && o < cap - 1; i++) {
    if (s[i] == '<') { while (i < n && s[i] != '>') i++; continue; }          // <tspan>
    if (s[i] == '&') {
      int j = i + 1; while (j < n && j < i + 10 && s[j] != ';') j++;
      if (j < n && s[j] == ';') {
        const char* e = s + i + 1; int en = j - i - 1; char c = 0;
        if (eq_n(e, en, "lt")) c = '<'; else if (eq_n(e, en, "gt")) c = '>'; else if (eq_n(e, en, "amp")) c = '&';
        else if (eq_n(e, en, "quot")) c = '"'; else if (eq_n(e, en, "apos")) c = '\'';
        else if (en > 1 && e[0] == '#') {
          uint32_t cp = 0;
          if (e[1] == 'x' || e[1] == 'X') { for (int k = 2; k < en; k++) cp = cp * 16 + (uint32_t)(hexv(e[k]) < 0 ? 0 : hexv(e[k])); }
          else for (int k = 1; k < en; k++) cp = cp * 10 + (uint32_t)(e[k] - '0');
          c = (char)unicode_to_local(cp);
        }
        if (c) { out[o++] = c; i = j; continue; }
      }
    }
    if (s[i] == '\n' || s[i] == '\r' || s[i] == '\t') { if (o && out[o - 1] != ' ') out[o++] = ' '; continue; }
    out[o++] = s[i];
  }
  while (o && out[o - 1] == ' ') o--;
  int st = 0; while (st < o && out[st] == ' ') st++;
  if (st) { for (int i = st; i < o; i++) out[i - st] = out[i]; o -= st; }
  out[o] = 0;
  return o;
}

static void text(const Attrs& a, const char* body, int bn, const Style& st) {
  if (!draw_text) return;
  char t[160]; int n = decode_entities(body, bn, t, sizeof t);
  if (!n) return;
  double fs = anum(a, "font-size", 14) * scale;
  int k = fs >= 22 ? 2 : 1;                                      // texto grande al doble
  int w = n * CH_W * k;
  const char* v; int vn;
  int x = R(TX(anum(a, "x", 0))), y = R(TY(anum(a, "y", 0)));
  if (attr(a, "text-anchor", &v, &vn)) { if (eq_n(v, vn, "middle")) x -= w / 2; else if (eq_n(v, vn, "end")) x -= w; }
  y -= 9 * k;                                                     // la línea base de la fuente
  uint16_t c = (uint16_t)(st.fill >= 0 ? st.fill : st.stroke >= 0 ? st.stroke : 0);
  for (int i = 0; i < n; i++) {
    const uint8_t* g = gfx_glyph((uint8_t)t[i]);
    for (int r = 0; r < 12; r++) for (int col = 0; col < 6; col++)
      if (g[r] & (0x80 >> col)) for (int dy = 0; dy < k; dy++) for (int dx = 0; dx < k; dx++) pset(x + (i * 6 + col) * k + dx, y + r * k + dy, c);
  }
}

// ------------------------------------------------------------------ recorrido del documento
static const char* parse_tag(const char* p, const char* e, const char** name, int* nn, Attrs* a, bool* closing, bool* selfclose) {
  // p apunta a '<'
  p++;
  *closing = false; *selfclose = false; a->n = 0;
  if (p < e && *p == '/') { *closing = true; p++; }
  *name = p; while (p < e && !sp(*p) && *p != '>' && *p != '/') p++;
  *nn = (int)(p - *name);
  while (p < e && *p != '>') {
    while (p < e && sp(*p)) p++;
    if (p < e && *p == '/') { *selfclose = true; p++; continue; }
    if (p >= e || *p == '>') break;
    const char* k = p; while (p < e && *p != '=' && !sp(*p) && *p != '>' && *p != '/') p++;
    int kn = (int)(p - k);
    while (p < e && sp(*p)) p++;
    if (p < e && *p == '=') {
      p++; while (p < e && sp(*p)) p++;
      char q = p < e ? *p : 0;
      const char* v; int vn;
      if (q == '"' || q == '\'') { p++; v = p; while (p < e && *p != q) p++; vn = (int)(p - v); if (p < e) p++; }
      else { v = p; while (p < e && !sp(*p) && *p != '>') p++; vn = (int)(p - v); }
      if (a->n < 24) { a->k[a->n] = k; a->kn[a->n] = kn; a->v[a->n] = v; a->vn[a->n] = vn; a->n++; }
    } else if (!kn) p++;
  }
  if (p < e) p++;
  return p;
}

void svg_render(const char* src, int n, int x, int y, int w, int h, bool with_text) {
  cx0 = x; cy0 = y; cw = w; ch = h; draw_text = with_text;
  for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (i >= 0 && j >= 0 && i < SCR_W && j < SCR_H) g_fb[j * SCR_W + i] = rgb(255, 255, 255);
  vbx = 0; vby = 0; double vbw = 320, vbh = 240;
  depth = 0; stk[0].fill = C_UNSET; stk[0].stroke = C_NONE; stk[0].sw = 1; stk[0].arrow = false;
  const char* p = src; const char* e = src + n;
  bool in_skip = false; int skip_depth = 0;
  while (p < e) {
    while (p < e && *p != '<') p++;
    if (p >= e) break;
    if (p + 4 <= e && p[1] == '!' && p[2] == '-' && p[3] == '-') {           // comentario
      while (p + 3 <= e && !(p[0] == '-' && p[1] == '-' && p[2] == '>')) p++;
      p += 3; continue;
    }
    if (p + 1 < e && (p[1] == '?' || p[1] == '!')) { while (p < e && *p != '>') p++; continue; }
    const char* name; int nn; Attrs a; bool closing, selfclose;
    p = parse_tag(p, e, &name, &nn, &a, &closing, &selfclose);
    bool skippable = eq_n(name, nn, "defs") || eq_n(name, nn, "style") || eq_n(name, nn, "script") ||
                     eq_n(name, nn, "marker") || eq_n(name, nn, "title") || eq_n(name, nn, "desc") ||
                     eq_n(name, nn, "clipPath") || eq_n(name, nn, "linearGradient") || eq_n(name, nn, "radialGradient") ||
                     eq_n(name, nn, "pattern") || eq_n(name, nn, "mask") || eq_n(name, nn, "symbol");
    if (in_skip) {
      if (skippable && !selfclose) skip_depth += closing ? -1 : 1;
      if (skip_depth <= 0) in_skip = false;
      continue;
    }
    if (skippable) { if (!closing && !selfclose) { in_skip = true; skip_depth = 1; } continue; }
    if (closing) { if (eq_n(name, nn, "g") && depth > 0) depth--; continue; }
    Style st = style_of(a, stk[depth]);
    if (eq_n(name, nn, "svg")) {
      const char* v; int vn;
      if (attr(a, "viewBox", &v, &vn)) {
        const char* q = v; double b[4];
        if (num(q, v + vn, &b[0]) && num(q, v + vn, &b[1]) && num(q, v + vn, &b[2]) && num(q, v + vn, &b[3]) && b[2] > 0 && b[3] > 0) {
          vbx = b[0]; vby = b[1]; vbw = b[2]; vbh = b[3];
        }
      } else { vbw = anum(a, "width", 320); vbh = anum(a, "height", 240); if (vbw <= 0) vbw = 320; if (vbh <= 0) vbh = 240; }
      double s1 = w / vbw, s2 = h / vbh;
      scale = s1 < s2 ? s1 : s2;
      ox = x + (w - vbw * scale) / 2; oy = y + (h - vbh * scale) / 2;
      stk[0] = st;
      continue;
    }
    if (eq_n(name, nn, "g")) { if (!selfclose && depth < 9) { stk[++depth] = st; } continue; }
    npts = 0;
    if (eq_n(name, nn, "line")) {
      Style ls = st; if (ls.stroke == C_NONE && ls.fill >= 0) ls.stroke = ls.fill;
      add_pt(anum(a, "x1", 0), anum(a, "y1", 0)); add_pt(anum(a, "x2", 0), anum(a, "y2", 0));
      ls.fill = C_NONE; paint(ls, false, false);
    } else if (eq_n(name, nn, "rect")) {
      double rx_ = anum(a, "x", 0), ry_ = anum(a, "y", 0), rw = anum(a, "width", 0), rh = anum(a, "height", 0);
      add_pt(rx_, ry_); add_pt(rx_ + rw, ry_); add_pt(rx_ + rw, ry_ + rh); add_pt(rx_, ry_ + rh);
      paint(st, true, true);
    } else if (eq_n(name, nn, "circle") || eq_n(name, nn, "ellipse")) {
      bool c = eq_n(name, nn, "circle");
      double ccx = anum(a, "cx", 0), ccy = anum(a, "cy", 0), rx_ = c ? anum(a, "r", 0) : anum(a, "rx", 0), ry_ = c ? rx_ : anum(a, "ry", 0);
      for (int i = 0; i < 48; i++) { double t = i * 6.28318530717959 / 48; add_pt(ccx + rx_ * m_cos(t), ccy + ry_ * m_sin(t)); }
      paint(st, true, true);
    } else if (eq_n(name, nn, "polyline") || eq_n(name, nn, "polygon")) {
      const char* v; int vn;
      if (attr(a, "points", &v, &vn)) { const char* q = v; double px_, py_; while (num(q, v + vn, &px_) && num(q, v + vn, &py_)) add_pt(px_, py_); }
      bool poly = eq_n(name, nn, "polygon");
      paint(st, poly, true);
    } else if (eq_n(name, nn, "path")) {
      const char* v; int vn;
      if (attr(a, "d", &v, &vn)) path(v, vn, st);
    } else if (eq_n(name, nn, "text")) {
      const char* body = p; const char* q = p;
      while (q + 6 < e && !(q[0] == '<' && q[1] == '/' && lower(q[2]) == 't' && lower(q[3]) == 'e' && lower(q[4]) == 'x' && lower(q[5]) == 't')) q++;
      if (!selfclose) { text(a, body, (int)(q - body), st); p = q; }
    }
    npts = 0;
  }
}

// ------------------------------------------------------------------ extraer <svg>...</svg> de un texto
int svg_find(const char* s, int n, int* start, int* end) {
  for (int i = 0; i + 4 < n; i++) {
    if (s[i] == '<' && lower(s[i + 1]) == 's' && lower(s[i + 2]) == 'v' && lower(s[i + 3]) == 'g' && (sp(s[i + 4]) || s[i + 4] == '>')) {
      for (int j = i + 4; j + 5 < n + 1; j++) {
        if (j + 6 <= n && s[j] == '<' && s[j + 1] == '/' && lower(s[j + 2]) == 's' && lower(s[j + 3]) == 'v' && lower(s[j + 4]) == 'g' && s[j + 5] == '>') {
          *start = i; *end = j + 6; return 1;
        }
      }
      *start = i; *end = n; return 1;                         // sin cierre: hasta el final
    }
  }
  return 0;
}
