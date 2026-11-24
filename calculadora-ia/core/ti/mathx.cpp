// mathx.cpp - Estado de la calculadora y funciones matemáticas (redondeo como la TI,
// ángulos, funciones especiales para distribuciones y el generador rand de la TI).
#include "ti_int.h"

TiState S;
bool graph_dirty = true;

const char* const ERR_NAME[E_COUNT] = {
  "", "SYNTAX", "DIVIDE BY 0", "DOMAIN", "NONREAL ANS", "OVERFLOW", "ARGUMENT", "DATA TYPE",
  "INVALID", "WINDOW RANGE", "NO SIGN CHNG", "BOUND", "TOL NOT MET", "MEMORY", "INCREMENT", "BAD GUESS",
  "UNDEFINED"
};

void state_defaults(bool all) {
  S.notation = 0; S.fix = -1; S.degree = 0; S.dotmode = 0; S.simul = 0;
  S.coord = 1; S.grid = 0; S.axes = 1; S.label = 0; S.expr = 1;
  S.xmin = -10; S.xmax = 10; S.xscl = 1; S.ymin = -10; S.ymax = 10; S.yscl = 1; S.xres = 1;
  S.xfact = 4; S.yfact = 4; S.tblstart = 0; S.dtbl = 1;
  if (all) {
    for (int i = 0; i < 10; i++) { S.y[i].n = 0; S.y[i].cur = 0; S.y[i].fresh = false; S.ysel[i] = 1; }
    for (int i = 0; i < 27; i++) S.vars[i] = 0;
    S.ans = 0;
    ti_seed(0);
  }
  graph_dirty = true;
}

// ------------------------------------------------------------------ utilidades
double pow10i(int n) {
  double r = 1, b = 10;
  bool neg = n < 0; if (neg) n = -n;
  while (n) { if (n & 1) r *= b; b *= b; n >>= 1; }
  return neg ? 1 / r : r;
}

static bool finite(double x) { return x == x && x - x == 0; }

// La TI trabaja con 14 cifras significativas: redondear cada resultado así evita
// errores como 0.1+0.2-0.3 = 5.5E-17 y deja sin(30°) = .5 exacto.
double r14(double x) {
  if (x == 0 || !finite(x)) return x;
  double a = m_fabs(x);
  if (a < 1e-99) return 0;
  int e = (int)m_floor(m_log10(a));
  double s = pow10i(13 - e);
  double y = m_floor(a * s + 0.5) / s;
  if (!finite(y)) return x;
  return x < 0 ? -y : y;
}

int chk(double x) {
  if (x != x) return E_DOMAIN;
  if (!finite(x) || x >= 1e100 || x <= -1e100) return E_OVERFLOW;
  return E_OK;
}

double fmodp(double a, double b) { double r = a - b * m_floor(a / b); return r; }
double ipart(double x) { return x < 0 ? -m_floor(-x) : m_floor(x); }

#define PI_D 3.14159265358979323846
double ang_to_rad(double x) { return S.degree ? x * (PI_D / 180) : x; }
double rad_to_ang(double r) { return S.degree ? r * (180 / PI_D) : r; }

// ------------------------------------------------------------------ funciones especiales
double lgam(double x) {                 // log Γ(x), Lanczos (g = 7)
  static const double c[9] = { 0.99999999999980993, 676.5203681218851, -1259.1392167224028,
    771.32342877765313, -176.61502916214059, 12.507343278686905, -0.13857109526572012,
    9.9843695780195716e-6, 1.5056327351493116e-7 };
  if (x < 0.5) return m_log(PI_D / m_fabs(m_sin(PI_D * x))) - lgam(1 - x);
  x -= 1;
  double a = c[0], t = x + 7.5;
  for (int i = 1; i < 9; i++) a += c[i] / (x + i);
  return 0.5 * m_log(2 * PI_D) + (x + 0.5) * m_log(t) - t + m_log(a);
}

// P(a, x) regularizada (serie o fracción continua)
double gammp(double a, double x) {
  if (x <= 0) return 0;
  double gln = lgam(a);
  if (x < a + 1) {
    double ap = a, del = 1 / a, sum = del;
    for (int n = 0; n < 2000; n++) { ap += 1; del *= x / ap; sum += del; if (m_fabs(del) < m_fabs(sum) * 1e-16) break; }
    return sum * m_exp(-x + a * m_log(x) - gln);
  }
  double b = x + 1 - a, c = 1e300, d = 1 / b, h = d;
  for (int i = 1; i < 2000; i++) {
    double an = -i * (i - a); b += 2;
    d = an * d + b; if (m_fabs(d) < 1e-300) d = 1e-300;
    c = b + an / c; if (m_fabs(c) < 1e-300) c = 1e-300;
    d = 1 / d; double del = d * c; h *= del;
    if (m_fabs(del - 1) < 1e-16) break;
  }
  return 1 - m_exp(-x + a * m_log(x) - gln) * h;
}

static double gammq_cf(double a, double x) {   // Q(a, x) para x grande (sin restar de 1)
  double gln = lgam(a);
  double b = x + 1 - a, c = 1e300, d = 1 / b, h = d;
  for (int i = 1; i < 2000; i++) {
    double an = -i * (i - a); b += 2;
    d = an * d + b; if (m_fabs(d) < 1e-300) d = 1e-300;
    c = b + an / c; if (m_fabs(c) < 1e-300) c = 1e-300;
    d = 1 / d; double del = d * c; h *= del;
    if (m_fabs(del - 1) < 1e-16) break;
  }
  return m_exp(-x + a * m_log(x) - gln) * h;
}

static double betacf(double a, double b, double x) {
  double qab = a + b, qap = a + 1, qam = a - 1, c = 1, d = 1 - qab * x / qap;
  if (m_fabs(d) < 1e-300) d = 1e-300;
  d = 1 / d; double h = d;
  for (int m = 1; m < 3000; m++) {
    int m2 = 2 * m;
    double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
    d = 1 + aa * d; if (m_fabs(d) < 1e-300) d = 1e-300;
    c = 1 + aa / c; if (m_fabs(c) < 1e-300) c = 1e-300;
    d = 1 / d; h *= d * c;
    aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
    d = 1 + aa * d; if (m_fabs(d) < 1e-300) d = 1e-300;
    c = 1 + aa / c; if (m_fabs(c) < 1e-300) c = 1e-300;
    d = 1 / d; double del = d * c; h *= del;
    if (m_fabs(del - 1) < 1e-16) break;
  }
  return h;
}

double betai(double a, double b, double x) {    // I_x(a, b)
  if (x <= 0) return 0;
  if (x >= 1) return 1;
  double bt = m_exp(lgam(a + b) - lgam(a) - lgam(b) + a * m_log(x) + b * m_log(1 - x));
  if (x < (a + 1) / (a + b + 2)) return bt * betacf(a, b, x) / a;
  return 1 - bt * betacf(b, a, 1 - x) / b;
}

// P(Z > z) para la normal estándar, precisa también en las colas
double normq(double z) {
  if (z < 0) return 1 - normq(-z);
  double t = z * z / 2;
  if (t == 0) return 0.5;
  double q = t < 1.5 ? 1 - gammp(0.5, t) : gammq_cf(0.5, t);
  return 0.5 * q;
}

// Inversa de la normal (Acklam) + un paso de Halley
double invnorm(double p) {
  static const double a[6] = { -3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
    1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00 };
  static const double b[5] = { -5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
    6.680131188771972e+01, -1.328068155288572e+01 };
  static const double c[6] = { -7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
    -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00 };
  static const double d[4] = { 7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
    3.754408661907416e+00 };
  double q, r, x;
  if (p < 0.02425) {
    q = m_sqrt(-2 * m_log(p));
    x = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
  } else if (p <= 1 - 0.02425) {
    q = p - 0.5; r = q * q;
    x = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q / (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
  } else {
    q = m_sqrt(-2 * m_log(1 - p));
    x = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
  }
  for (int i = 0; i < 2; i++) {
    double e = (1 - normq(x)) - p;
    double u = e * m_sqrt(2 * PI_D) * m_exp(x * x / 2);
    x = x - u / (1 + x * u / 2);
  }
  return x;
}

// ------------------------------------------------------------------ rand (algoritmo de L'Ecuyer, igual que la TI)
#define MOD1 2147483563LL
#define MOD2 2147483399LL
void ti_seed(double n) {
  int64_t k = (int64_t)m_fabs(ipart(n));
  if (k == 0) { S.seed1 = 12345; S.seed2 = 67890; return; }
  S.seed1 = (40014LL * k) % MOD1;
  S.seed2 = k % MOD2;
}
double ti_rand(void) {
  S.seed1 = (S.seed1 * 40014LL) % MOD1;
  S.seed2 = (S.seed2 * 40692LL) % MOD2;
  double r = (double)(S.seed1 - S.seed2) / (double)MOD1;
  if (r < 0) r += 1;
  return r;
}
