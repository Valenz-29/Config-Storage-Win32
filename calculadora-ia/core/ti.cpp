// ti.cpp - Pantalla de inicio estilo TI-83 Plus (96x64 pixeles, 16x8 caracteres),
// dibujada ampliada x3 en la TFT para que se vea como la pantalla original.
#include "ti.h"
#include "gfx.h"
#include "keys.h"
#include "hal.h"

// ------------------------------------------------------------------ tokens
enum {
  T_DOT = 10, T_EE, T_ADD, T_SUB, T_MUL, T_DIV, T_POW, T_LP, T_RP, T_NEG,
  T_SQR, T_INV, T_SIN, T_COS, T_TAN, T_ASIN, T_ACOS, T_ATAN, T_LOG, T_LN,
  T_TENPOW, T_EPOW, T_SQRT, T_PI, T_E, T_ANS, T_STO, T_COMMA,
  T_VAR  // T_VAR + 0..25 = A..Z, T_VAR + 26 = θ
};
#define NVARS 27

static const char* tok_str(uint8_t t, char* tmp) {
  static const char* S[] = {
    ".", "\x85", "+", "-", "*", "/", "^", "(", ")", "\x86",
    "\xB2", "\x80", "sin(", "cos(", "tan(", "sin\x80(", "cos\x80(", "tan\x80(", "log(", "ln(",
    "10^(", "e^(", "\x83(", "\x84", "e", "Ans", "\x82", ","
  };
  if (t <= 9) { tmp[0] = (char)('0' + t); tmp[1] = 0; return tmp; }
  if (t >= T_VAR) { tmp[0] = (char)(t - T_VAR < 26 ? 'A' + (t - T_VAR) : CH_THETA); tmp[1] = 0; return tmp; }
  return S[t - T_DOT];
}

// ------------------------------------------------------------------ estado
#define COLS 16
#define ROWS 8
#define MAXTOK 120

static bool on = true;
static bool second = false, alpha = false;
static uint8_t entry[MAXTOK]; static int elen = 0;
static uint8_t last_entry[MAXTOK]; static int last_len = 0;
static char rows[ROWS][COLS + 1]; static int nrows = 0;   // líneas ya escritas
static double ans = 0, vars[NVARS];
static int err = 0;  // != 0 -> pantalla de error
static bool blink = true; static uint32_t blink_t = 0;

static const char* ERR_NAMES[] = { "", "SYNTAX", "DIVIDE BY 0", "DOMAIN", "NONREAL ANS", "OVERFLOW" };
enum { E_OK, E_SYNTAX, E_DIV0, E_DOMAIN, E_NONREAL, E_OVERFLOW };

static void push_row(const char* s, bool right) {
  if (nrows == ROWS) { for (int i = 1; i < ROWS; i++) m_copy(rows[i - 1], rows[i], COLS + 1); nrows--; }
  char* r = rows[nrows++];
  for (int i = 0; i < COLS; i++) r[i] = ' ';
  r[COLS] = 0;
  int n = s_len(s); if (n > COLS) n = COLS;
  int off = right ? COLS - n : 0;
  for (int i = 0; i < n; i++) r[off + i] = s[i];
}

// Texto de la entrada actual en el juego de caracteres local
static int entry_text(const uint8_t* t, int n, char* out, int cap) {
  int o = 0; char tmp[2];
  for (int i = 0; i < n; i++) {
    const char* s = tok_str(t[i], tmp);
    while (*s && o < cap - 1) out[o++] = *s++;
  }
  out[o] = 0;
  return o;
}

// ------------------------------------------------------------------ formato de números (como la TI)
static void fmt_num(double v, char* out) {
  int o = 0;
  if (v != v) { s_cpy(out, "?", 4); return; }
  if (v < 0) { out[o++] = (char)CH_NEG; v = -v; }
  if (v == 0) { out[o++] = '0'; out[o] = 0; return; }
  int e = (int)m_floor(m_log10(v));
  double m = v / m_pow(10, e);
  long long d = (long long)(m * 1e9 + 0.5);       // 10 cifras significativas
  if (d >= 10000000000LL) { d /= 10; e++; }
  if (d < 1000000000LL) { d *= 10; e--; }
  char dig[11];
  for (int i = 9; i >= 0; i--) { dig[i] = (char)('0' + d % 10); d /= 10; }
  int nd = 10; while (nd > 1 && dig[nd - 1] == '0') nd--;
  if (e >= 10 || e < -3) {                        // notación científica
    out[o++] = dig[0];
    if (nd > 1) { out[o++] = '.'; for (int i = 1; i < nd; i++) out[o++] = dig[i]; }
    out[o++] = (char)CH_EE;
    if (e < 0) { out[o++] = (char)CH_NEG; e = -e; }
    if (e >= 10) out[o++] = (char)('0' + e / 10);
    out[o++] = (char)('0' + e % 10);
  } else if (e >= 0) {
    for (int i = 0; i <= e; i++) out[o++] = i < nd ? dig[i] : '0';
    if (nd > e + 1) { out[o++] = '.'; for (int i = e + 1; i < nd; i++) out[o++] = dig[i]; }
  } else {                                        // 0.00123 -> .00123
    out[o++] = '.';
    for (int i = 0; i < -e - 1; i++) out[o++] = '0';
    for (int i = 0; i < nd; i++) out[o++] = dig[i];
  }
  out[o] = 0;
}

// ------------------------------------------------------------------ evaluador
static const uint8_t* P; static int PN, PI_;
static int perr;

static int peek() { return PI_ < PN ? P[PI_] : -1; }
static bool starts_primary(int t) {
  return (t >= 0 && t <= 9) || t == T_DOT || t == T_LP || (t >= T_SIN && t <= T_SQRT) ||
         t == T_PI || t == T_E || t == T_ANS || t >= T_VAR || t == T_NEG;
}
static double fail(int e) { if (!perr) perr = e; return 0; }

static double parse_sum();
static double parse_neg();

static double parse_number() {
  double v = 0, scale = 0; bool any = false;
  while (PI_ < PN && (P[PI_] <= 9 || P[PI_] == T_DOT)) {
    int t = P[PI_++];
    if (t == T_DOT) { if (scale) return fail(E_SYNTAX); scale = 1; continue; }
    any = true;
    if (scale) { scale /= 10; v += t * scale; } else v = v * 10 + t;
  }
  if (peek() == T_EE) {
    PI_++;
    bool neg = false; if (peek() == T_NEG) { neg = true; PI_++; }
    int ex = 0; bool d = false;
    while (PI_ < PN && P[PI_] <= 9) { ex = ex * 10 + P[PI_++]; d = true; }
    if (!d) return fail(E_SYNTAX);
    if (!any) v = 1;
    v *= m_pow(10, neg ? -ex : ex);
    any = true;
  }
  if (!any) return fail(E_SYNTAX);
  return v;
}

static void close_paren() { if (peek() == T_RP) PI_++; else if (PI_ < PN) fail(E_SYNTAX); }

static double parse_primary() {
  int t = peek();
  if (t < 0) return fail(E_SYNTAX);
  if (t <= 9 || t == T_DOT || t == T_EE) return parse_number();
  PI_++;
  if (t == T_LP) { double v = parse_sum(); close_paren(); return v; }
  if (t == T_PI) return 3.14159265358979323846;
  if (t == T_E) return 2.71828182845904523536;
  if (t == T_ANS) return ans;
  if (t >= T_VAR) return vars[t - T_VAR];
  if (t >= T_SIN && t <= T_SQRT) {
    double x = parse_sum(); close_paren();
    if (perr) return 0;
    switch (t) {
      case T_SIN: return m_sin(x);
      case T_COS: return m_cos(x);
      case T_TAN: return m_tan(x);
      case T_ASIN: if (x < -1 || x > 1) return fail(E_DOMAIN); return m_asin(x);
      case T_ACOS: if (x < -1 || x > 1) return fail(E_DOMAIN); return m_acos(x);
      case T_ATAN: return m_atan(x);
      case T_LOG: if (x <= 0) return fail(x == 0 ? E_DOMAIN : E_NONREAL); return m_log10(x);
      case T_LN: if (x <= 0) return fail(x == 0 ? E_DOMAIN : E_NONREAL); return m_log(x);
      case T_TENPOW: return m_pow(10, x);
      case T_EPOW: return m_exp(x);
      case T_SQRT: if (x < 0) return fail(E_NONREAL); return m_sqrt(x);
    }
  }
  return fail(E_SYNTAX);
}

static double parse_postfix() {
  double v = parse_primary();
  for (;;) {
    int t = peek();
    if (t == T_SQR) { PI_++; v = v * v; }
    else if (t == T_INV) { PI_++; if (v == 0) return fail(E_DIV0); v = 1 / v; }
    else break;
  }
  return v;
}

static double powr(double a, double b) {
  if (a == 0 && b == 0) return fail(E_DOMAIN);
  if (a == 0 && b < 0) return fail(E_DIV0);
  if (a < 0 && m_floor(b) != b) return fail(E_NONREAL);
  return m_pow(a, b);
}

static double parse_pow() {
  double v = parse_postfix();
  while (peek() == T_POW) { PI_++; double b = parse_neg(); v = powr(v, b); }
  return v;
}

static double parse_neg() {
  if (peek() == T_NEG) { PI_++; return -parse_neg(); }
  return parse_pow();
}

static double parse_term() {
  double v = parse_neg();
  for (;;) {
    int t = peek();
    if (t == T_MUL) { PI_++; v *= parse_neg(); }
    else if (t == T_DIV) { PI_++; double d = parse_neg(); if (d == 0) return fail(E_DIV0); v /= d; }
    else if (starts_primary(t)) v *= parse_neg();   // multiplicación implícita: 2π, 3(4)
    else break;
    if (perr) return 0;
  }
  return v;
}

static double parse_sum() {
  double v = parse_term();
  for (;;) {
    int t = peek();
    if (t == T_ADD) { PI_++; v += parse_term(); }
    else if (t == T_SUB) { PI_++; v -= parse_term(); }
    else break;
    if (perr) return 0;
  }
  return v;
}

static int evaluate(const uint8_t* t, int n, double* out) {
  P = t; PN = n; PI_ = 0; perr = 0;
  // "5→A": guarda en una variable
  int store = -1;
  if (n >= 2 && t[n - 2] == T_STO && t[n - 1] >= T_VAR) { store = t[n - 1] - T_VAR; PN = n - 2; }
  double v = parse_sum();
  if (!perr && PI_ < PN) perr = E_SYNTAX;
  if (!perr && (v != v)) perr = E_DOMAIN;
  if (!perr && (v >= 1e100 || v <= -1e100)) perr = E_OVERFLOW;
  if (perr) return perr;
  if (store >= 0) vars[store] = v;
  *out = v;
  return 0;
}

// ------------------------------------------------------------------ teclas
static void add_tok(uint8_t t) {
  // Si la entrada empieza con un operador, la TI pone "Ans" adelante
  if (elen == 0 && (t == T_ADD || t == T_SUB || t == T_MUL || t == T_DIV || t == T_POW ||
                    t == T_SQR || t == T_INV || t == T_STO)) entry[elen++] = T_ANS;
  if (elen < MAXTOK) entry[elen++] = t;
}

static void do_enter() {
  char buf[256];
  if (elen == 0) {                       // ENTER vacío: repite la última entrada
    if (!last_len) return;
    m_copy(entry, last_entry, last_len); elen = last_len;
  } else {
    int n = entry_text(entry, elen, buf, sizeof buf);
    for (int i = 0; i < n; i += COLS) {
      char row[COLS + 1]; int k = n - i < COLS ? n - i : COLS;
      m_copy(row, buf + i, k); row[k] = 0;
      push_row(row, false);
    }
    m_copy(last_entry, entry, elen); last_len = elen;
  }
  double v;
  int e = evaluate(entry, elen, &v);
  if (e) { err = e; return; }
  ans = v;
  fmt_num(v, buf);
  push_row(buf, true);
  elen = 0;
}

static uint8_t key_token(int k) {
  if (alpha) {
    uint8_t a = alpha_of(k);
    if (a >= 'A' && a <= 'Z') return (uint8_t)(T_VAR + (a - 'A'));
    if (a == CH_THETA) return T_VAR + 26;
    return 0xFF;
  }
  if (second) {
    switch (k) {
      case K_SIN: return T_ASIN; case K_COS: return T_ACOS; case K_TAN: return T_ATAN;
      case K_SQR: return T_SQRT; case K_LOG: return T_TENPOW; case K_LN: return T_EPOW;
      case K_POW: return T_PI;   case K_DIV: return T_E;      case K_COMMA: return T_EE;
      case K_NEG: return T_ANS;
    }
    return 0xFF;
  }
  if (k >= K_7 && k <= K_9) return (uint8_t)(7 + k - K_7);
  if (k >= K_4 && k <= K_6) return (uint8_t)(4 + k - K_4);
  if (k >= K_1 && k <= K_3) return (uint8_t)(1 + k - K_1);
  switch (k) {
    case K_0: return 0;         case K_DOT: return T_DOT;   case K_ADD: return T_ADD;
    case K_SUB: return T_SUB;   case K_MUL: return T_MUL;   case K_DIV: return T_DIV;
    case K_POW: return T_POW;   case K_LPAREN: return T_LP; case K_RPAREN: return T_RP;
    case K_NEG: return T_NEG;   case K_SQR: return T_SQR;   case K_INV: return T_INV;
    case K_SIN: return T_SIN;   case K_COS: return T_COS;   case K_TAN: return T_TAN;
    case K_LOG: return T_LOG;   case K_LN: return T_LN;     case K_STO: return T_STO;
    case K_COMMA: return T_COMMA;
  }
  return 0xFF;
}

void ti_init(void) { on = true; elen = 0; nrows = 0; err = 0; }
bool ti_is_on(void) { return on; }

void ti_key(int k) {
  g_dirty = true; blink = true; blink_t = hal_millis();
  if (!on) { if (k == K_ON) { on = true; second = alpha = false; } return; }
  if (err) {                                   // pantalla de error: 1:Quit 2:Goto
    if (k == K_1 || k == K_ENTER || k == K_CLEAR) { err = 0; elen = 0; }
    else if (k == K_2) { err = 0; }
    return;
  }
  if (k == K_ON) { if (second) { on = false; second = false; } return; }  // 2nd+ON = apagar
  if (k == K_2ND) { second = !second; alpha = false; return; }
  if (k == K_ALPHA) { alpha = !alpha; second = false; return; }
  bool was2nd = second; second = false;
  if (k == K_ENTER) {
    alpha = false;
    if (was2nd) { m_copy(entry, last_entry, last_len); elen = last_len; return; }  // ENTRY
    do_enter(); return;
  }
  if (k == K_CLEAR) { alpha = false; if (elen) elen = 0; else nrows = 0; return; }
  if (k == K_DEL) { if (elen) elen--; return; }
  if (was2nd) second = true;                   // key_token necesita saber si venía con 2nd
  uint8_t t = key_token(k);
  second = false; alpha = false;
  if (t != 0xFF) add_tok(t);
}

void ti_tick(uint32_t now) {
  if (on && !err && now - blink_t >= 500) { blink = !blink; blink_t = now; g_dirty = true; }
}

// ------------------------------------------------------------------ dibujo
#define LCD_X 16
#define LCD_Y 24
#define PIX 3
static const uint16_t BEZEL = 0x2104;  // gris oscuro (marco)

static uint16_t lcd_bg() { return rgb(158, 172, 140); }
static uint16_t lcd_px() { return rgb(28, 34, 30); }

static void lcd_cell(int col, int row, uint8_t ch, bool inverse) {
  const uint8_t* g = gfx_glyph(ch);
  int r0 = (ch == 'g' || ch == 'p' || ch == 'q' || ch == 'y' || ch == 'j') ? 2 : 0;  // sin descendente
  uint16_t on_c = lcd_px(), off_c = lcd_bg();
  for (int r = 0; r < 8; r++) {
    uint8_t bits = (r + r0 < 12) ? g[r + r0] : 0;
    for (int c = 0; c < 6; c++) {
      bool px = (bits & (0x80 >> c)) != 0;
      if (inverse) px = !px;  // celda completa en negativo (como el cursor y el "1:" de la TI)
      if (px) gfx_fill(LCD_X + (col * 6 + c) * PIX, LCD_Y + (row * 8 + r) * PIX, PIX, PIX, on_c);
      else if (inverse) gfx_fill(LCD_X + (col * 6 + c) * PIX, LCD_Y + (row * 8 + r) * PIX, PIX, PIX, off_c);
    }
  }
}

static void lcd_row(int row, const char* s) {
  for (int i = 0; s[i] && i < COLS; i++) if (s[i] != ' ') lcd_cell(i, row, (uint8_t)s[i], false);
}

void ti_draw(void) {
  gfx_fill(0, 0, SCR_W, SCR_H, BEZEL);
  gfx_fill(LCD_X, LCD_Y, 96 * PIX, 64 * PIX, lcd_bg());
  if (!on) return;
  if (err) {
    char l[COLS + 1] = "ERR:";
    s_cpy(l + 4, ERR_NAMES[err], COLS - 3);
    lcd_row(0, l);
    lcd_cell(0, 1, '1', true); lcd_cell(1, 1, ':', true); lcd_row(1, "  Quit");
    lcd_row(2, "2:Goto");
    return;
  }
  char buf[256];
  int n = entry_text(entry, elen, buf, sizeof buf);
  int erows = n / COLS + 1;
  if (erows > ROWS) erows = ROWS;
  int show = ROWS - erows; if (show > nrows) show = nrows;
  int y = 0;
  for (int i = nrows - show; i < nrows; i++) lcd_row(y++, rows[i]);
  int start = n - (n % COLS) - (erows - 1) * COLS; if (start < 0) start = 0;
  for (int i = start, c = 0; i < n; i++, c++) lcd_cell(c % COLS, y + c / COLS, (uint8_t)buf[i], false);
  if (blink) {
    int c = n - start;
    uint8_t cur = second ? CH_UP : alpha ? 'A' : ' ';
    lcd_cell(c % COLS, y + c / COLS, cur, true);
  }
}
