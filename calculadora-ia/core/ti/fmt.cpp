// fmt.cpp - Cómo la TI-83 Plus muestra los números: Float/Fix, Normal/Sci/Eng, ►Frac y ►DMS.
#include "ti_int.h"

// Descompone a > 0 en "sig" cifras redondeadas. Devuelve el exponente decimal.
static int decomp(double a, int sig, char* dig) {
  int e = (int)m_floor(m_log10(a));
  double m = a / pow10i(e);
  if (m >= 10) { m /= 10; e++; } else if (m < 1) { m *= 10; e--; }
  double n = m_floor(m * pow10i(sig - 1) + 0.5);
  if (n >= pow10i(sig)) { n = m_floor(n / 10 + 0.5); e++; }
  for (int i = sig - 1; i >= 0; i--) { double q = m_floor(n / 10); dig[i] = (char)('0' + (int)(n - q * 10)); n = q; }
  return e;
}

static int strip(const char* dig, int nd) { while (nd > 1 && dig[nd - 1] == '0') nd--; return nd; }

static int put_int(char* out, int o, int v) {   // entero sin signo
  char tmp[12]; int k = 0;
  do { tmp[k++] = (char)('0' + v % 10); v /= 10; } while (v);
  while (k) out[o++] = tmp[--k];
  return o;
}

static int put_exp(char* out, int o, int e) {
  out[o++] = (char)CH_EE;
  if (e < 0) { out[o++] = (char)CH_NEG; e = -e; }
  return put_int(out, o, e);
}

// Mantisa con "intd" cifras enteras (1 para Sci, 1..3 para Eng) y "nd" cifras en total
static int build_sci(bool neg, const char* dig, int nd, int intd, int e, int minfrac, char* out) {
  int o = 0;
  if (neg) out[o++] = (char)CH_NEG;
  for (int i = 0; i < intd; i++) out[o++] = i < nd ? dig[i] : '0';
  int frac = nd - intd; if (frac < minfrac) frac = minfrac;
  if (frac > 0) { out[o++] = '.'; for (int i = 0; i < frac; i++) out[o++] = intd + i < nd ? dig[intd + i] : '0'; }
  o = put_exp(out, o, e - (intd - 1));
  out[o] = 0;
  return o;
}

// Notación normal con las cifras dadas (sin el 0 antes del punto, como la TI)
static int build_fixed(bool neg, const char* dig, int nd, int e, int minfrac, char* out) {
  int o = 0;
  if (neg) out[o++] = (char)CH_NEG;
  int fracd;
  if (e >= 0) {
    for (int i = 0; i <= e; i++) out[o++] = i < nd ? dig[i] : '0';
    fracd = nd - (e + 1); if (fracd < minfrac) fracd = minfrac;
    if (fracd > 0) { out[o++] = '.'; for (int i = 0; i < fracd; i++) out[o++] = e + 1 + i < nd ? dig[e + 1 + i] : '0'; }
  } else {
    out[o++] = '.';
    int zeros = -e - 1;
    for (int i = 0; i < zeros; i++) out[o++] = '0';
    fracd = zeros + nd; if (fracd < minfrac) fracd = minfrac;
    for (int i = 0; i < fracd - zeros; i++) out[o++] = i < nd ? dig[i] : '0';
  }
  out[o] = 0;
  return o;
}

int fmt_mode(double v, char* out, int maxlen, int notation, int fix) {
  char dig[16];
  (void)maxlen;
  if (v != v) { s_cpy(out, "?", 4); return 1; }
  bool neg = v < 0;
  double a = neg ? -v : v;
  if (a == 0) {
    int o = 0;
    out[o++] = '0';
    if (fix > 0) { out[o++] = '.'; for (int i = 0; i < fix; i++) out[o++] = '0'; }
    if (notation && fix >= 0) { out[o++] = (char)CH_EE; out[o++] = '0'; }
    out[o] = 0;
    return o;
  }
  if (notation == 1) {                                   // Sci
    if (fix < 0) { int e = decomp(a, 10, dig); return build_sci(neg, dig, strip(dig, 10), 1, e, 0, out); }
    int e = decomp(a, fix + 1, dig);
    return build_sci(neg, dig, fix + 1, 1, e, fix, out);
  }
  if (notation == 2) {                                   // Eng
    int e = decomp(a, 14, dig);
    for (int pass = 0; pass < 2; pass++) {
      int e3 = (int)m_floor(e / 3.0) * 3, intd = e - e3 + 1;
      int sig = fix < 0 ? 10 : intd + fix; if (sig > 10) sig = 10;
      int e2 = decomp(a, sig, dig);
      if (e2 != e) { e = e2; continue; }
      int nd = fix < 0 ? strip(dig, sig) : sig;
      return build_sci(neg, dig, nd, intd, e, fix < 0 ? 0 : fix, out);
    }
  }
  if (fix < 0) {                                         // Normal Float
    int e0 = (int)m_floor(m_log10(a));
    int sig = e0 >= 0 ? 10 : 11 + e0; if (sig < 1) sig = 1; if (sig > 10) sig = 10;
    int e = decomp(a, sig, dig);
    if (e >= 10 || e < -3) { e = decomp(a, 10, dig); return build_sci(neg, dig, strip(dig, 10), 1, e, 0, out); }
    return build_fixed(neg, dig, strip(dig, sig), e, 0, out);
  }
  // Normal Fix N
  int e0 = decomp(a, 14, dig);
  if (e0 < 10) {
    int dec = fix;
    if (e0 >= 0 && e0 + 1 + dec > 10) dec = 10 - (e0 + 1);
    double n = m_floor(a * pow10i(dec) + 0.5);
    if (n > 0 && n < 1e10) {
      int o = 0; char tmp[20]; int k = 0;
      while (n >= 1) { double q = m_floor(n / 10); tmp[k++] = (char)('0' + (int)(n - q * 10)); n = q; }
      while (k < dec) tmp[k++] = '0';                    // ceros después del punto
      if (neg) out[o++] = (char)CH_NEG;
      for (int i = k - 1; i >= dec; i--) out[o++] = tmp[i];
      if (dec > 0) { out[o++] = '.'; for (int i = dec - 1; i >= 0; i--) out[o++] = tmp[i]; }
      out[o] = 0;
      return o;
    }
  }
  int e = decomp(a, fix + 1, dig);                        // no cabe: científica con N decimales
  return build_sci(neg, dig, fix + 1, 1, e, fix, out);
}

// Formato en un modo dado; si no cabe en maxlen, usa menos cifras.
static int fmt_fit(double v, char* out, int maxlen, int notation, int fix) {
  int n = fmt_mode(v, out, maxlen, notation, fix);
  if (n <= maxlen) return n;
  char dig[16];
  bool neg = v < 0; double a = neg ? -v : v;
  for (int sig = 9; sig >= 1; sig--) {
    int e = decomp(a, sig, dig);
    int nd = strip(dig, sig);
    if (notation == 0 && e < 10 && e >= -3) {
      n = build_fixed(neg, dig, nd, e, 0, out);
      if (n <= maxlen) return n;
    }
    n = build_sci(neg, dig, nd, 1, e, 0, out);
    if (n <= maxlen) return n;
  }
  out[maxlen] = 0;
  return maxlen;
}

int fmt_num(double v, char* out, int maxlen) { return fmt_fit(v, out, maxlen, S.notation, S.fix); }

// ►Frac: fracción con denominador menor que 10000, si existe
int fmt_frac(double v, char* out, int maxlen) {
  bool neg = v < 0; double x = neg ? -v : v;
  if (x >= 1e10) return 0;
  if (x == m_floor(x)) return fmt_num(v, out, maxlen);
  double h0 = 0, h1 = 1, k0 = 1, k1 = 0, b = x;
  for (int it = 0; it < 40; it++) {
    double a = m_floor(b), h2 = a * h1 + h0, k2 = a * k1 + k0;
    if (k2 >= 10000) return 0;
    if (m_fabs(h2 / k2 - x) <= 1e-12 * x) {
      int o = 0;
      if (neg) out[o++] = (char)CH_NEG;
      char t[16]; int n = fmt_mode(h2, t, 16, 0, -1);
      for (int i = 0; i < n; i++) out[o++] = t[i];
      out[o++] = '/';
      n = fmt_mode(k2, t, 16, 0, -1);
      for (int i = 0; i < n; i++) out[o++] = t[i];
      out[o] = 0;
      return o <= maxlen ? o : 0;
    }
    h0 = h1; h1 = h2; k0 = k1; k1 = k2;
    double f = b - a;
    if (f < 1e-15) return 0;
    b = 1 / f;
  }
  return 0;
}

// ►DMS: grados°minutos'segundos"
int fmt_dms(double v, char* out, int maxlen) {
  bool neg = v < 0; double a = neg ? -v : v;
  if (a >= 1e10) return 0;
  double D = m_floor(a), rem = (a - D) * 60, M = m_floor(rem), Sx = r14((rem - M) * 60);
  if (Sx >= 59.9999999) { Sx = 0; M += 1; }
  if (M >= 60) { M = 0; D += 1; }
  int o = 0; char t[20];
  if (neg) out[o++] = (char)CH_NEG;
  int n = fmt_mode(D, t, 16, 0, -1); for (int i = 0; i < n; i++) out[o++] = t[i];
  out[o++] = (char)0xB0;
  n = fmt_mode(M, t, 16, 0, -1); for (int i = 0; i < n; i++) out[o++] = t[i];
  out[o++] = '\'';
  int room = maxlen - o - 1; if (room < 1) room = 1;
  n = fmt_fit(Sx, t, room, 0, -1);
  for (int i = 0; i < n; i++) out[o++] = t[i];
  out[o++] = '"';
  out[o] = 0;
  return o;
}

// Número -> tokens (para RCL y los valores de WINDOW)
int num_tokens(double v, uint8_t* t, int cap) {
  char buf[24]; int n = fmt_mode(v, buf, 16, 0, -1), k = 0;
  for (int i = 0; i < n && k < cap; i++) {
    uint8_t c = (uint8_t)buf[i];
    if (c >= '0' && c <= '9') t[k++] = (uint8_t)(c - '0');
    else if (c == '.') t[k++] = T_DOT;
    else if (c == CH_NEG) t[k++] = T_NEG;
    else if (c == CH_EE) t[k++] = T_EE;
  }
  return k;
}
