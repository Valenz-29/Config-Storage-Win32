// eval.cpp - Evaluador con el orden de operaciones de la TI-83 Plus:
//  1 funciones con paréntesis   2 postfijos (² ⁻¹ ! ° ʳ)   3 ^ y ˣ√   4 negación (‾)
//  5 nPr nCr   6 * / e implícita   7 + -   8 = ≠ > ≥ < ≤   9 and   10 or xor
#include "ti_int.h"

struct P { const uint8_t* t; int pos, end; };

static int g_err = 0, g_errpos = 0, g_depth = 0;

static double fail(P& p, int e) { if (!g_err) { g_err = e; g_errpos = p.pos; } return 0; }
static int peek(P& p) { return p.pos < p.end ? p.t[p.pos] : -1; }
static bool stmt_end(P& p) {
  int t = peek(p);
  return t < 0 || t == T_COLON || t == T_STO || tok((uint8_t)t).kind == TK_CONV;
}
static double ok(P& p, double v) {           // redondeo a 14 cifras + desbordamiento
  int e = chk(v);
  if (e) return fail(p, e);
  return r14(v);
}

static double e_or(P& p);
static double e_neg(P& p);
static double e_post(P& p);

static void close_paren(P& p) {
  if (peek(p) == T_RP) { p.pos++; return; }
  if (!stmt_end(p)) fail(p, E_SYNTAX);       // la TI permite omitir los ) al final
}

// ------------------------------------------------------------------ números
static double number(P& p) {
  double mant = 0; int dec = 0; bool any = false, dot = false;
  while (p.pos < p.end && (p.t[p.pos] <= T_9 || p.t[p.pos] == T_DOT)) {
    int t = p.t[p.pos++];
    if (t == T_DOT) { if (dot) return fail(p, E_SYNTAX); dot = true; continue; }
    any = true;
    mant = mant * 10 + t;
    if (dot) dec++;
  }
  double v = any ? mant / pow10i(dec) : 1;
  if (peek(p) == T_EE) {
    p.pos++;
    bool neg = false;
    if (peek(p) == T_NEG) { neg = true; p.pos++; }
    int ex = 0, nd = 0;
    while (p.pos < p.end && p.t[p.pos] <= T_9) { ex = ex * 10 + p.t[p.pos++]; if (++nd > 3) return fail(p, E_SYNTAX); }
    if (!nd) return fail(p, E_SYNTAX);
    v *= pow10i(neg ? -ex : ex);
  } else if (!any) return fail(p, E_SYNTAX);
  return ok(p, v);
}

// ------------------------------------------------------------------ trigonometría con grados exactos
#define PI_D 3.14159265358979323846
static double trig(P& p, int f, double x) {
  if (S.degree) {
    double d = fmodp(x, 360), q = d / 90;
    if (q == m_floor(q)) {
      int k = (int)q % 4;
      if (f == T_SIN) return k == 1 ? 1 : k == 3 ? -1 : 0;
      if (f == T_COS) return k == 0 ? 1 : k == 2 ? -1 : 0;
      if (k == 1 || k == 3) return fail(p, E_DOMAIN);
      return 0;
    }
    x = d * (PI_D / 180);
  } else if (m_fabs(x) >= 1e14) return fail(p, E_DOMAIN);
  double r = f == T_SIN ? m_sin(x) : f == T_COS ? m_cos(x) : m_tan(x);
  if (m_fabs(r) < 1e-13 && m_fabs(x) > 1e-6) r = 0;     // sin(π) = 0, como la TI
  return r;
}

static double atan2r(double y, double x) {
  if (x > 0) return m_atan(y / x);
  if (x < 0) return m_atan(y / x) + (y >= 0 ? PI_D : -PI_D);
  return y > 0 ? PI_D / 2 : y < 0 ? -PI_D / 2 : 0;
}

// ------------------------------------------------------------------ potencias (con raíces impares de negativos)
static bool odd_denominator(double b, double* num) {
  // b = n/d con d impar pequeño (por ejemplo 1/3, 2/5)
  for (int d = 1; d <= 999; d += 2) {
    double n = b * d, rn = m_floor(n + 0.5);
    if (m_fabs(n - rn) < 1e-10 * (m_fabs(n) + 1)) { *num = rn; return true; }
  }
  return false;
}

static double powr(P& p, double a, double b) {
  if (a == 0) {
    if (b == 0) return fail(p, E_DOMAIN);
    if (b < 0) return fail(p, E_DIV0);
    return 0;
  }
  if (a < 0 && b != m_floor(b)) {
    double num;
    if (!odd_denominator(b, &num)) return fail(p, E_NONREAL);
    double r = m_pow(-a, b);
    return ok(p, ((int64_t)num % 2) ? -r : r);
  }
  return ok(p, m_pow(a, b));
}

static double factorial(P& p, double x) {
  if (x < -0.5 || x * 2 != m_floor(x * 2)) return fail(p, E_DOMAIN);
  if (x > 69.5) return fail(p, E_OVERFLOW);
  double r = 1;
  if (x == m_floor(x)) { for (int i = 2; i <= (int)x; i++) r *= i; return ok(p, r); }
  r = 1.7724538509055160273;                       // (-.5)! = √π
  for (double k = 0.5; k <= x + 1e-9; k += 1) r *= k;
  return ok(p, r);
}

static bool is_int(double x) { return x == m_floor(x); }

// ------------------------------------------------------------------ distribuciones
static double norm_between(double l, double u) {   // P(l < Z < u)
  if (l >= 0) return normq(l) - normq(u);
  if (u <= 0) return normq(-u) - normq(-l);
  return 1 - normq(-l) - normq(u);
}
static double t_upper(double t, double df) {      // P(T > t)
  double x = df / (df + t * t);
  double h = 0.5 * betai(df / 2, 0.5, x);
  return t >= 0 ? h : 1 - h;
}
static double binom_pdf(double n, double pr, double x) {
  if (x < 0 || x > n) return 0;
  if (pr == 0) return x == 0 ? 1 : 0;
  if (pr == 1) return x == n ? 1 : 0;
  if (n <= 1000) {
    double c = 1; double k = x < n - x ? x : n - x;
    for (int i = 1; i <= (int)k; i++) c = c * (n - k + i) / i;
    return c * m_pow(pr, x) * m_pow(1 - pr, n - x);
  }
  return m_exp(lgam(n + 1) - lgam(x + 1) - lgam(n - x + 1) + x * m_log(pr) + (n - x) * m_log(1 - pr));
}
static double poisson_pdf(double mu, double x) {
  if (x < 0) return 0;
  if (x <= 170) { double r = m_exp(-mu); for (int i = 1; i <= (int)x; i++) r = r * mu / i; return r; }
  return m_exp(-mu + x * m_log(mu) - lgam(x + 1));
}

// ------------------------------------------------------------------ funciones con expresión como argumento
static double call_expr(P& p, int f, int mn, int mx) {
  int s = p.pos, depth = 0, i = s;
  for (; i < p.end; i++) {
    int t = p.t[i];
    if (t == T_LP || tok((uint8_t)t).kind == TK_FUNC) depth++;
    else if (t == T_RP) { if (!depth) break; depth--; }
    else if (!depth && (t == T_COMMA || t == T_COLON || t == T_STO)) break;
  }
  if (i >= p.end || p.t[i] != T_COMMA) { p.pos = i; return fail(p, E_ARGUMENT); }
  int es = s, ee = i;
  p.pos = i + 1;
  int vt = peek(p);
  if (vt < 0 || tok((uint8_t)vt).kind != TK_VAR) return fail(p, E_SYNTAX);
  int var = tok((uint8_t)vt).arg;
  p.pos++;
  if (peek(p) != T_COMMA) return fail(p, E_ARGUMENT);
  p.pos++;
  double a[3]; int n = 0;
  for (;;) {
    if (n >= 3) return fail(p, E_ARGUMENT);
    a[n++] = e_or(p);
    if (g_err) return 0;
    if (peek(p) == T_COMMA) { p.pos++; continue; }
    break;
  }
  close_paren(p);
  if (g_err) return 0;
  if (n + 2 < mn || n + 2 > mx) return fail(p, E_ARGUMENT);
  int err = 0;
  double r = 0;
  if (f == T_NDERIV) {
    double h = n > 1 ? a[1] : 1e-3, y1, y2;
    if (h == 0) return fail(p, E_DOMAIN);
    err = eval_at(p.t + es, ee - es, var, a[0] + h, &y1);
    if (!err) err = eval_at(p.t + es, ee - es, var, a[0] - h, &y2);
    if (!err) r = (y1 - y2) / (2 * h);
  } else if (f == T_FNINT) {
    r = integrate(p.t + es, ee - es, var, a[0], a[1], &err);
  } else {
    if (a[0] >= a[1]) return fail(p, E_BOUND);
    r = minimize(p.t + es, ee - es, var, a[0], a[1], f == T_FMAX, &err);
  }
  if (err) return fail(p, err);
  return ok(p, r);
}

// ------------------------------------------------------------------ funciones
static double call(P& p, int f) {
  const TokInfo& ti = tok((uint8_t)f);
  int mn = ti.arg & 15, mx = ti.arg >> 4;
  if (f == T_FMIN || f == T_FMAX || f == T_NDERIV || f == T_FNINT) return call_expr(p, f, mn, mx);
  double a[5]; int n = 0;
  for (;;) {
    if (n >= 5) return fail(p, E_ARGUMENT);
    a[n++] = e_or(p);
    if (g_err) return 0;
    if (peek(p) == T_COMMA) { p.pos++; continue; }
    break;
  }
  close_paren(p);
  if (g_err) return 0;
  if (n < mn || n > mx) return fail(p, E_ARGUMENT);
  double x = a[0];
  switch (f) {
    case T_SIN: case T_COS: case T_TAN: return ok(p, trig(p, f, x));
    case T_ASIN: if (x < -1 || x > 1) return fail(p, E_DOMAIN); return ok(p, rad_to_ang(m_asin(x)));
    case T_ACOS: if (x < -1 || x > 1) return fail(p, E_DOMAIN); return ok(p, rad_to_ang(m_acos(x)));
    case T_ATAN: return ok(p, rad_to_ang(m_atan(x)));
    case T_SINH: return ok(p, (m_exp(x) - m_exp(-x)) / 2);
    case T_COSH: return ok(p, (m_exp(x) + m_exp(-x)) / 2);
    case T_TANH: { if (x > 20) return 1; if (x < -20) return -1; double e2 = m_exp(2 * x); return ok(p, (e2 - 1) / (e2 + 1)); }
    case T_ASINH: return ok(p, x < 0 ? -m_log(-x + m_sqrt(x * x + 1)) : m_log(x + m_sqrt(x * x + 1)));
    case T_ACOSH: if (x < 1) return fail(p, E_NONREAL); return ok(p, m_log(x + m_sqrt(x * x - 1)));
    case T_ATANH: if (x <= -1 || x >= 1) return fail(p, E_DOMAIN); return ok(p, 0.5 * m_log((1 + x) / (1 - x)));
    case T_LOG: if (x == 0) return fail(p, E_DOMAIN); if (x < 0) return fail(p, E_NONREAL); return ok(p, m_log10(x));
    case T_LN: if (x == 0) return fail(p, E_DOMAIN); if (x < 0) return fail(p, E_NONREAL); return ok(p, m_log(x));
    case T_TENPOW: return powr(p, 10, x);
    case T_EPOW: return ok(p, m_exp(x));
    case T_SQRT: if (x < 0) return fail(p, E_NONREAL); return ok(p, m_sqrt(x));
    case T_CBRT: return ok(p, x < 0 ? -m_pow(-x, 1.0 / 3) : m_pow(x, 1.0 / 3));
    case T_ABS: return m_fabs(x);
    case T_ROUND: {
      if (n == 1) {
        if (x == 0) return 0;
        int e = (int)m_floor(m_log10(m_fabs(x)));
        double s = pow10i(9 - e);
        return ok(p, (x < 0 ? -1 : 1) * m_floor(m_fabs(x) * s + 0.5) / s);
      }
      double d = a[1];
      if (!is_int(d) || d < 0 || d > 9) return fail(p, E_DOMAIN);
      double s = pow10i((int)d);
      return ok(p, (x < 0 ? -1 : 1) * m_floor(m_fabs(x) * s + 0.5) / s);
    }
    case T_IPART: return ipart(x);
    case T_FPART: return ok(p, x - ipart(x));
    case T_INT: return m_floor(x);
    case T_MINF: return a[0] < a[1] ? a[0] : a[1];
    case T_MAXF: return a[0] > a[1] ? a[0] : a[1];
    case T_LCM: case T_GCD: {
      double u = a[0], v = a[1];
      if (!is_int(u) || !is_int(v) || u < 0 || v < 0 || u >= 1e12 || v >= 1e12) return fail(p, E_DOMAIN);
      double g1 = u, g2 = v;
      while (g2 > 0) { double r = g1 - g2 * m_floor(g1 / g2); g1 = g2; g2 = r; }
      if (f == T_GCD) return g1;
      return g1 == 0 ? 0 : ok(p, u / g1 * v);
    }
    case T_RANDINT: {
      double lo = a[0], hi = a[1];
      if (!is_int(lo) || !is_int(hi)) return fail(p, E_DOMAIN);
      if (lo > hi) { double t = lo; lo = hi; hi = t; }
      return m_floor((hi - lo + 1) * ti_rand()) + lo;
    }
    case T_RANDNORM: {
      double r1 = ti_rand(), r2 = ti_rand();
      if (r1 <= 0) r1 = 1e-12;
      return ok(p, a[0] + a[1] * m_sqrt(-2 * m_log(r1)) * m_cos(2 * PI_D * r2));
    }
    case T_RANDBIN: {
      double nn = a[0], pr = a[1];
      if (!is_int(nn) || nn < 1 || pr < 0 || pr > 1) return fail(p, E_DOMAIN);
      int c = 0; for (int i = 0; i < (int)nn && i < 100000; i++) if (ti_rand() < pr) c++;
      return c;
    }
    case T_NOT: return x == 0 ? 1 : 0;
    case T_RPR: return ok(p, m_sqrt(a[0] * a[0] + a[1] * a[1]));
    case T_RPT: return ok(p, rad_to_ang(atan2r(a[1], a[0])));
    case T_PRX: { double c = trig(p, T_COS, a[1]); return ok(p, a[0] * c); }
    case T_PRY: { double s = trig(p, T_SIN, a[1]); return ok(p, a[0] * s); }
    // --- distribuciones
    case T_NORMPDF: {
      if (n == 2) return fail(p, E_ARGUMENT);
      double mu = n > 1 ? a[1] : 0, sg = n > 2 ? a[2] : 1;
      if (sg <= 0) return fail(p, E_DOMAIN);
      double z = (x - mu) / sg;
      return ok(p, m_exp(-z * z / 2) / (sg * 2.5066282746310002));
    }
    case T_NORMCDF: {
      if (n == 3) return fail(p, E_ARGUMENT);
      double mu = n > 2 ? a[2] : 0, sg = n > 3 ? a[3] : 1;
      if (sg <= 0) return fail(p, E_DOMAIN);
      if (a[0] > a[1]) return fail(p, E_DOMAIN);
      return ok(p, norm_between((a[0] - mu) / sg, (a[1] - mu) / sg));
    }
    case T_INVNORM: {
      if (n == 2) return fail(p, E_ARGUMENT);
      double mu = n > 1 ? a[1] : 0, sg = n > 2 ? a[2] : 1;
      if (x <= 0 || x >= 1 || sg <= 0) return fail(p, E_DOMAIN);
      return ok(p, mu + sg * invnorm(x));
    }
    case T_TPDF: {
      double df = a[1]; if (df <= 0) return fail(p, E_DOMAIN);
      return ok(p, m_exp(lgam((df + 1) / 2) - lgam(df / 2) - (df + 1) / 2 * m_log(1 + x * x / df)) / m_sqrt(df * PI_D));
    }
    case T_TCDF: {
      double df = a[2]; if (df <= 0 || a[0] > a[1]) return fail(p, E_DOMAIN);
      double l = a[0], u = a[1];
      if (l >= 0) return ok(p, t_upper(l, df) - t_upper(u, df));
      if (u <= 0) return ok(p, t_upper(-u, df) - t_upper(-l, df));
      return ok(p, 1 - t_upper(-l, df) - t_upper(u, df));
    }
    case T_CHIPDF: {
      double df = a[1]; if (df <= 0) return fail(p, E_DOMAIN);
      if (x < 0) return 0;
      if (x == 0) return df == 2 ? 0.5 : df > 2 ? 0 : fail(p, E_DOMAIN);
      return ok(p, m_exp((df / 2 - 1) * m_log(x) - x / 2 - df / 2 * 0.69314718055994531 - lgam(df / 2)));
    }
    case T_CHICDF: {
      double df = a[2]; if (df <= 0 || a[0] > a[1]) return fail(p, E_DOMAIN);
      double fu = a[1] > 0 ? gammp(df / 2, a[1] / 2) : 0, fl = a[0] > 0 ? gammp(df / 2, a[0] / 2) : 0;
      return ok(p, fu - fl);
    }
    case T_FPDF: {
      double n1 = a[1], d1 = a[2]; if (n1 <= 0 || d1 <= 0) return fail(p, E_DOMAIN);
      if (x < 0) return 0;
      if (x == 0) return n1 == 2 ? 1 : n1 > 2 ? 0 : fail(p, E_DOMAIN);
      return ok(p, m_exp(lgam((n1 + d1) / 2) - lgam(n1 / 2) - lgam(d1 / 2) + n1 / 2 * m_log(n1 / d1) +
                         (n1 / 2 - 1) * m_log(x) - (n1 + d1) / 2 * m_log(1 + n1 * x / d1)));
    }
    case T_FCDF: {
      double n1 = a[2], d1 = a[3]; if (n1 <= 0 || d1 <= 0 || a[0] > a[1]) return fail(p, E_DOMAIN);
      double fu = a[1] > 0 ? betai(n1 / 2, d1 / 2, n1 * a[1] / (n1 * a[1] + d1)) : 0;
      double fl = a[0] > 0 ? betai(n1 / 2, d1 / 2, n1 * a[0] / (n1 * a[0] + d1)) : 0;
      return ok(p, fu - fl);
    }
    case T_BINPDF: case T_BINCDF: {
      double nn = a[0], pr = a[1], k = a[2];
      if (!is_int(nn) || nn < 0 || pr < 0 || pr > 1) return fail(p, E_DOMAIN);
      if (f == T_BINPDF) { if (!is_int(k)) return fail(p, E_DOMAIN); return ok(p, binom_pdf(nn, pr, k)); }
      if (k < 0) return 0;
      if (k >= nn) return 1;
      double s = 0; for (int i = 0; i <= (int)m_floor(k); i++) s += binom_pdf(nn, pr, i);
      return ok(p, s > 1 ? 1 : s);
    }
    case T_POIPDF: case T_POICDF: {
      double mu = a[0], k = a[1];
      if (mu <= 0) return fail(p, E_DOMAIN);
      if (f == T_POIPDF) { if (!is_int(k)) return fail(p, E_DOMAIN); return ok(p, poisson_pdf(mu, k)); }
      if (k < 0) return 0;
      double s = 0; for (int i = 0; i <= (int)m_floor(k) && i < 100000; i++) s += poisson_pdf(mu, i);
      return ok(p, s > 1 ? 1 : s);
    }
    case T_GEOPDF: case T_GEOCDF: {
      double pr = a[0], k = a[1];
      if (pr <= 0 || pr > 1 || !is_int(k) || k < 1) return fail(p, E_DOMAIN);
      if (f == T_GEOPDF) return ok(p, pr * m_pow(1 - pr, k - 1));
      return ok(p, 1 - m_pow(1 - pr, k));
    }
  }
  return fail(p, E_SYNTAX);
}

// ------------------------------------------------------------------ variables del sistema
static double svar_get(int i) {
  switch (i) {
    case 0: return S.xmin; case 1: return S.xmax; case 2: return S.xscl; case 3: return S.ymin;
    case 4: return S.ymax; case 5: return S.yscl; case 6: return S.xres;
    case 7: return r14((S.xmax - S.xmin) / 94); case 8: return r14((S.ymax - S.ymin) / 62);
    case 9: return S.xfact; case 10: return S.yfact; case 11: return S.tblstart; case 12: return S.dtbl;
  }
  return 0;
}
static int svar_set(int i, double v) {
  switch (i) {
    case 0: S.xmin = v; break; case 1: S.xmax = v; break; case 2: S.xscl = v; break;
    case 3: S.ymin = v; break; case 4: S.ymax = v; break; case 5: S.yscl = v; break;
    case 6: if (!is_int(v) || v < 1 || v > 8) return E_DOMAIN; S.xres = v; break;
    case 7: S.xmax = r14(S.xmin + 94 * v); break;
    case 8: S.ymax = r14(S.ymin + 62 * v); break;
    case 9: if (v < 1 || v > 100) return E_DOMAIN; S.xfact = v; break;
    case 10: if (v < 1 || v > 100) return E_DOMAIN; S.yfact = v; break;
    case 11: S.tblstart = v; break;
    case 12: if (v == 0) return E_INCREMENT; S.dtbl = v; break;
  }
  graph_dirty = true;
  return E_OK;
}

// ------------------------------------------------------------------ niveles del evaluador
static double e_prim(P& p) {
  int t = peek(p);
  if (t < 0) return fail(p, E_SYNTAX);
  const TokInfo& ti = tok((uint8_t)t);
  switch (ti.kind) {
    case TK_NUM: return number(p);
    case TK_LP: { p.pos++; double v = e_or(p); if (g_err) return 0; close_paren(p); return v; }
    case TK_FUNC: p.pos++; return call(p, t);
    case TK_CONST:
      p.pos++;
      if (t == T_PI) return 3.1415926535898;
      if (t == T_E) return 2.718281828459;
      if (t == T_RAND) return ti_rand();
      return S.ans;
    case TK_VAR: p.pos++; return S.vars[ti.arg];
    case TK_SVAR: p.pos++; return svar_get(ti.arg);
    case TK_YVAR: {
      p.pos++;
      Editor& y = S.y[ti.arg];
      if (!y.n) return fail(p, E_UNDEFINED);
      double x = S.vars[V_X], r;
      if (peek(p) == T_LP) { p.pos++; x = e_or(p); if (g_err) return 0; close_paren(p); if (g_err) return 0; }
      int e = eval_at(y.t, y.n, V_X, x, &r);
      if (e) return fail(p, e);
      return r;
    }
  }
  return fail(p, E_SYNTAX);
}

static double e_post(P& p) {
  double v = e_prim(p);
  while (!g_err) {
    int t = peek(p);
    if (t == T_SQR) { p.pos++; v = ok(p, v * v); }
    else if (t == T_CUBE) { p.pos++; v = ok(p, v * v * v); }
    else if (t == T_INV) { p.pos++; if (v == 0) return fail(p, E_DIV0); v = ok(p, 1 / v); }
    else if (t == T_FACT) { p.pos++; v = factorial(p, v); }
    else if (t == T_RADS) { p.pos++; if (S.degree) v = ok(p, v * (180 / PI_D)); }
    else if (t == T_DEGS || t == T_MINS) {
      p.pos++;
      double deg = t == T_DEGS ? v : v / 60;
      if (t == T_DEGS) {                       // grados°minutos'segundos"
        int save = p.pos; int nt = peek(p);
        if (nt >= 0 && (nt <= T_9 || nt == T_DOT)) {
          double m = number(p);
          if (!g_err && peek(p) == T_MINS) {
            p.pos++; deg += m / 60;
            int s2 = p.pos; nt = peek(p);
            if (nt >= 0 && (nt <= T_9 || nt == T_DOT)) {
              double sec = number(p);
              if (!g_err && peek(p) == T_QUOTE) { p.pos++; deg += sec / 3600; } else { p.pos = s2; }
            }
          } else { p.pos = save; }
        }
      }
      v = ok(p, S.degree ? deg : deg * (PI_D / 180));
    }
    else break;
  }
  return v;
}

static double e_pow(P& p) {
  double v = e_post(p);
  while (!g_err) {
    int t = peek(p);
    if (t != T_POW && t != T_XROOT) break;
    p.pos++;
    bool neg = false;
    while (peek(p) == T_NEG) { neg = !neg; p.pos++; }
    double w = e_post(p);
    if (g_err) return 0;
    if (neg) w = -w;
    if (t == T_POW) v = powr(p, v, w);
    else {                                     // índice ˣ√ radicando
      if (v == 0) return fail(p, E_DOMAIN);
      v = powr(p, w, 1 / v);
    }
  }
  return v;
}

static double e_neg(P& p) {
  if (peek(p) == T_NEG) { p.pos++; return -e_neg(p); }
  return e_pow(p);
}

static double e_npr(P& p) {
  double v = e_neg(p);
  while (!g_err) {
    int t = peek(p);
    if (t != T_NPR && t != T_NCR) break;
    p.pos++;
    double r = e_neg(p);
    if (g_err) return 0;
    if (!is_int(v) || !is_int(r) || v < 0 || r < 0) return fail(p, E_DOMAIN);
    if (r > v) { v = 0; continue; }
    double res = 1;
    if (t == T_NPR) for (double i = 0; i < r; i++) res *= v - i;
    else { double k = r < v - r ? r : v - r; for (double i = 1; i <= k; i++) res = res * (v - k + i) / i; res = m_floor(res + 0.5); }
    v = ok(p, res);
  }
  return v;
}

static bool implicit_start(int t) {
  if (t < 0) return false;
  int k = tok((uint8_t)t).kind;
  return k == TK_NUM || k == TK_LP || k == TK_FUNC || k == TK_CONST || k == TK_VAR || k == TK_YVAR ||
         k == TK_SVAR || k == TK_NEG;
}

static double e_mul(P& p) {
  double v = e_npr(p);
  while (!g_err) {
    int t = peek(p);
    if (t == T_MUL) { p.pos++; double w = e_npr(p); v = ok(p, v * w); }
    else if (t == T_DIV) { p.pos++; double w = e_npr(p); if (g_err) return 0; if (w == 0) return fail(p, E_DIV0); v = ok(p, v / w); }
    else if (implicit_start(t)) { double w = e_npr(p); v = ok(p, v * w); }
    else break;
  }
  return v;
}

static double e_add(P& p) {
  double v = e_mul(p);
  while (!g_err) {
    int t = peek(p);
    if (t == T_ADD) { p.pos++; double w = e_mul(p); v = ok(p, v + w); }
    else if (t == T_SUB) { p.pos++; double w = e_mul(p); v = ok(p, v - w); }
    else break;
  }
  return v;
}

static double e_rel(P& p) {
  double v = e_add(p);
  while (!g_err) {
    int t = peek(p);
    if (t < T_EQ || t > T_LE) break;
    p.pos++;
    double w = e_add(p);
    switch (t) {
      case T_EQ: v = v == w; break; case T_NE: v = v != w; break;
      case T_GT: v = v > w; break;  case T_GE: v = v >= w; break;
      case T_LT: v = v < w; break;  case T_LE: v = v <= w; break;
    }
  }
  return v;
}

static double e_and(P& p) {
  double v = e_rel(p);
  while (!g_err && peek(p) == T_AND) { p.pos++; double w = e_rel(p); v = (v != 0) && (w != 0); }
  return v;
}

static double e_or(P& p) {
  double v = e_and(p);
  while (!g_err) {
    int t = peek(p);
    if (t != T_OR && t != T_XOR) break;
    p.pos++;
    double w = e_and(p);
    v = t == T_OR ? ((v != 0) || (w != 0)) : ((v != 0) != (w != 0));
  }
  return v;
}

// ------------------------------------------------------------------ sentencias
static bool stmt_done(P& p) { int t = peek(p); return t < 0 || t == T_COLON; }

static void run_stmt(P& p, Stmt* out) {
  out->conv = 0; out->done = false; out->v = 0;
  int t = peek(p);
  if (t >= 0 && tok((uint8_t)t).kind == TK_CMD) {
    p.pos++;
    if (t == T_CLRENT) home_clear_entries();
    else {                                     // FnOn / FnOff [n,n,...]
      bool on = t == T_FNON;
      if (stmt_done(p)) { for (int i = 0; i < 10; i++) S.ysel[i] = on; }
      else for (;;) {
        int d = peek(p);
        if (d < 0 || d > T_9) { fail(p, E_SYNTAX); return; }
        p.pos++;
        S.ysel[d == 0 ? 9 : d - 1] = on;
        if (peek(p) == T_COMMA) { p.pos++; continue; }
        break;
      }
      graph_dirty = true;
    }
    if (!stmt_done(p)) { fail(p, E_SYNTAX); return; }
    out->done = true;
    return;
  }
  double v = e_or(p);
  if (g_err) return;
  t = peek(p);
  if (t >= 0 && tok((uint8_t)t).kind == TK_CONV) { out->conv = (uint8_t)(t - T_FRAC + 1); p.pos++; t = peek(p); }
  if (t == T_STO) {
    p.pos++;
    int d = peek(p);
    if (d < 0) { fail(p, E_SYNTAX); return; }
    const TokInfo& ti = tok((uint8_t)d);
    p.pos++;
    if (ti.kind == TK_VAR) S.vars[ti.arg] = v;
    else if (ti.kind == TK_SVAR) { int e = svar_set(ti.arg, v); if (e) { p.pos--; fail(p, e); return; } }
    else if (d == T_RAND) ti_seed(v);
    else if (ti.kind == TK_YVAR) { p.pos--; fail(p, E_DATATYPE); return; }
    else { p.pos--; fail(p, E_SYNTAX); return; }
    t = peek(p);
    if (t >= 0 && tok((uint8_t)t).kind == TK_CONV) { out->conv = (uint8_t)(t - T_FRAC + 1); p.pos++; }
  }
  if (!stmt_done(p)) { fail(p, E_SYNTAX); return; }
  out->v = v;
}

int eval_line(const uint8_t* t, int n, Stmt* out, int* errpos) {
  P p = { t, 0, n };
  g_err = 0; g_errpos = 0; g_depth = 0;
  out->v = 0; out->conv = 0; out->done = false;
  if (n == 0) return E_SYNTAX;
  while (p.pos < p.end) {
    run_stmt(p, out);
    if (g_err) break;
    if (!out->done) S.ans = out->v;
    if (peek(p) == T_COLON) p.pos++;
  }
  if (g_err) { if (errpos) *errpos = g_errpos > n ? n : g_errpos; return g_err; }
  return E_OK;
}

int eval_expr(const uint8_t* t, int n, double* v, int* errpos) {
  int save_err = g_err, save_pos = g_errpos;
  if (++g_depth > 12) { g_depth--; return E_MEMORY; }
  g_err = 0; g_errpos = 0;
  P p = { t, 0, n };
  double r = 0;
  if (n == 0) g_err = E_SYNTAX;
  else {
    r = e_or(p);
    if (!g_err && p.pos < p.end) fail(p, E_SYNTAX);
  }
  int e = g_err;
  if (errpos) *errpos = g_errpos;
  g_err = save_err; g_errpos = save_pos; g_depth--;
  if (!e) *v = r;
  return e;
}

int eval_at(const uint8_t* t, int n, int var, double x, double* y) {
  double old = S.vars[var];
  S.vars[var] = x;
  int e = eval_expr(t, n, y, 0);
  S.vars[var] = old;
  return e;
}

int eval_fn(int idx, double x, double* y) { return eval_at(S.y[idx].t, S.y[idx].n, V_X, x, y); }

// ------------------------------------------------------------------ integral (Gauss-Kronrod adaptativo)
static const double XGK[8] = { 0.991455371120812639206854697526329, 0.949107912342758524526189684047851,
  0.864864423359769072789712788640926, 0.741531185599394439863864773280788, 0.586087235467691130294144845693013,
  0.405845151377397166906606412076961, 0.207784955007898467600689403773245, 0 };
static const double WGK[8] = { 0.022935322010529224963732008058970, 0.063092092629978553290700663189204,
  0.104790010322250183839876322541518, 0.140653259715525918745189590510238, 0.169004726639267902826583426598550,
  0.190350578064785409913256402421014, 0.204432940075298892414161999234649, 0.209482141084727828012999174891714 };
static const double WG[4] = { 0.129484966168869693270611432679082, 0.279705391489276667901467771423780,
  0.381830050505118944950369775488975, 0.417959183673469387755102040816327 };

struct Integ { const uint8_t* t; int n, var, err, evals; };

static double fx(Integ& q, double x) {
  double y;
  int e = eval_at(q.t, q.n, q.var, x, &y);
  if (e && !q.err) q.err = e;
  q.evals++;
  return e ? 0 : y;
}

static double gk15(Integ& q, double a, double b, double* errest) {
  double c = (a + b) / 2, h = (b - a) / 2, fc = fx(q, c);
  double rk = fc * WGK[7], rg = fc * WG[3];
  for (int j = 0; j < 7; j++) {
    double d = h * XGK[j], f1 = fx(q, c - d), f2 = fx(q, c + d);
    rk += WGK[j] * (f1 + f2);
    if (j & 1) rg += WG[j / 2] * (f1 + f2);
  }
  *errest = m_fabs((rk - rg) * h);
  return rk * h;
}

static double adapt(Integ& q, double a, double b, double whole, double err, double tol, int depth) {
  if (q.err) return 0;
  if (err <= tol || depth > 24 || q.evals > 60000) return whole;
  double m = (a + b) / 2, e1, e2;
  double l = gk15(q, a, m, &e1), r = gk15(q, m, b, &e2);
  return adapt(q, a, m, l, e1, tol / 2, depth + 1) + adapt(q, m, b, r, e2, tol / 2, depth + 1);
}

double integrate(const uint8_t* t, int n, int var, double a, double b, int* err) {
  Integ q = { t, n, var, 0, 0 };
  if (a == b) { *err = 0; return 0; }
  double e, whole = gk15(q, a, b, &e);
  double tol = 1e-11 * (m_fabs(whole) > 1 ? m_fabs(whole) : 1);
  double r = adapt(q, a, b, whole, e, tol, 0);
  *err = q.err;
  return r;
}

// ------------------------------------------------------------------ mínimo/máximo (método de Brent)
double minimize(const uint8_t* t, int n, int var, double a, double b, bool maxi, int* err) {
  Integ q = { t, n, var, 0, 0 };
  double sg = maxi ? -1 : 1;
  const double C = 0.3819660112501051;
  double x = a + C * (b - a), w = x, v = x, fxv = sg * fx(q, x), fw = fxv, fv = fxv, d = 0, e = 0;
  for (int it = 0; it < 200 && !q.err; it++) {
    double m = (a + b) / 2, tol1 = 1e-10 * m_fabs(x) + 1e-12, tol2 = 2 * tol1;
    if (m_fabs(x - m) <= tol2 - (b - a) / 2) break;
    bool golden = true;
    if (m_fabs(e) > tol1) {
      double r = (x - w) * (fxv - fv), qq = (x - v) * (fxv - fw), pp = (x - v) * qq - (x - w) * r;
      qq = 2 * (qq - r);
      if (qq > 0) pp = -pp; else qq = -qq;
      double etemp = e; e = d;
      if (m_fabs(pp) < m_fabs(0.5 * qq * etemp) && pp > qq * (a - x) && pp < qq * (b - x)) {
        d = pp / qq; double u = x + d;
        if (u - a < tol2 || b - u < tol2) d = x < m ? tol1 : -tol1;
        golden = false;
      }
    }
    if (golden) { e = (x >= m ? a : b) - x; d = C * e; }
    double u = m_fabs(d) >= tol1 ? x + d : x + (d > 0 ? tol1 : -tol1);
    double fu = sg * fx(q, u);
    if (fu <= fxv) {
      if (u >= x) a = x; else b = x;
      v = w; fv = fw; w = x; fw = fxv; x = u; fxv = fu;
    } else {
      if (u < x) a = u; else b = u;
      if (fu <= fw || w == x) { v = w; fv = fw; w = u; fw = fu; }
      else if (fu <= fv || v == x || v == w) { v = u; fv = fu; }
    }
  }
  *err = q.err;
  return x;
}
