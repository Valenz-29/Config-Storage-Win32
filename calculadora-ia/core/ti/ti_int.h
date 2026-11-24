// ti_int.h - Piezas internas del modo calculadora normal (estilo TI-83 Plus).
#pragma once
#include <stdint.h>
#include "../gfx.h"
#include "../keys.h"
#include "../hal.h"

// ===================================================================== LCD 96x64
#define LCD_W 96
#define LCD_H 64
extern uint8_t lcd_px[LCD_H][LCD_W];
extern uint8_t lcd_chars[8][16];          // texto grande en pantalla (para pruebas)
extern char lcd_log[512];                 // textos chicos dibujados (para pruebas)
extern int lcd_log_n;
void lcd_clear(void);
void lcd_set(int x, int y, int v);
int  lcd_get(int x, int y);
void lcd_char(int col, int row, uint8_t ch, bool inv = false);
void lcd_text(int col, int row, const char* s, int n, bool inv = false);
void lcd_str(int col, int row, const char* s, bool inv = false);
void lcd_glyph_px(int x, int y, uint8_t ch, int r0, int nrows, bool inv);
int  lcd_small(int x, int y, const char* s, int n, bool inv = false);
int  lcd_small_str(int x, int y, const char* s, bool inv = false);
void lcd_line(int x0, int y0, int x1, int y1, int v = 1);
void lcd_fill(int x, int y, int w, int h, int v);
void lcd_invert(int x, int y, int w, int h);
void lcd_blit(void);

// ===================================================================== tokens
enum : uint8_t {
  T_0 = 0, T_9 = 9,
  T_DOT, T_EE, T_NEG,
  T_ADD, T_SUB, T_MUL, T_DIV, T_POW, T_XROOT, T_NPR, T_NCR,
  T_EQ, T_NE, T_GT, T_GE, T_LT, T_LE, T_AND, T_OR, T_XOR,
  T_LP, T_RP, T_COMMA, T_COLON, T_STO, T_SPACE, T_QUOTE, T_QMARK,
  T_SQR, T_INV, T_CUBE, T_FACT, T_DEGS, T_RADS, T_MINS,
  T_FRAC, T_DEC, T_DMS,
  T_SIN, T_COS, T_TAN, T_ASIN, T_ACOS, T_ATAN,
  T_SINH, T_COSH, T_TANH, T_ASINH, T_ACOSH, T_ATANH,
  T_LOG, T_LN, T_TENPOW, T_EPOW, T_SQRT, T_CBRT,
  T_ABS, T_ROUND, T_IPART, T_FPART, T_INT, T_MINF, T_MAXF, T_LCM, T_GCD,
  T_FMIN, T_FMAX, T_NDERIV, T_FNINT,
  T_RANDINT, T_RANDNORM, T_RANDBIN, T_NOT,
  T_RPR, T_RPT, T_PRX, T_PRY,
  T_NORMPDF, T_NORMCDF, T_INVNORM, T_TPDF, T_TCDF, T_CHIPDF, T_CHICDF, T_FPDF, T_FCDF,
  T_BINPDF, T_BINCDF, T_POIPDF, T_POICDF, T_GEOPDF, T_GEOCDF,
  T_PI, T_E, T_RAND, T_ANS,
  T_VA, T_VTHETA = T_VA + 26,
  T_Y1, T_Y0 = T_Y1 + 9,
  T_XMIN, T_XMAX, T_XSCL, T_YMIN, T_YMAX, T_YSCL, T_XRES, T_DX, T_DY, T_XFACT, T_YFACT, T_TBLSTART, T_DTBL,
  T_CLRENT, T_FNON, T_FNOFF,
  T_COUNT
};
#define T_VX (T_VA + 23)
#define T_VY (T_VA + 24)
#define V_X 23
#define V_Y 24

enum : uint8_t { TK_NUM, TK_NEG, TK_BIN, TK_LP, TK_RP, TK_COMMA, TK_COLON, TK_STO, TK_MISC,
                 TK_POST, TK_CONV, TK_FUNC, TK_CONST, TK_VAR, TK_YVAR, TK_SVAR, TK_CMD };
struct TokInfo { const char* text; uint8_t kind; uint8_t arg; };
const TokInfo& tok(uint8_t t);
int tok_len(uint8_t t);

// ===================================================================== estado
#define MAXTOK 120
struct Editor { uint8_t t[MAXTOK]; int n; int cur; bool fresh; };

struct TiState {
  uint8_t notation;            // 0 Normal, 1 Sci, 2 Eng
  int8_t  fix;                 // -1 Float, 0..9 decimales fijos
  uint8_t degree;              // 0 Radian, 1 Degree
  uint8_t dotmode;             // 0 Connected, 1 Dot
  uint8_t simul;               // 0 Sequential, 1 Simul
  uint8_t coord, grid, axes, label, expr;   // FORMAT (1 = encendido)
  double xmin, xmax, xscl, ymin, ymax, yscl, xres, xfact, yfact;
  double tblstart, dtbl;
  Editor y[10];                // Y1..Y9, Y0
  uint8_t ysel[10];
  double vars[27];             // A..Z, θ
  double ans;
  int64_t seed1, seed2;
};
extern TiState S;
void state_defaults(bool all);
extern bool graph_dirty;

// ===================================================================== matemáticas
enum { E_OK, E_SYNTAX, E_DIV0, E_DOMAIN, E_NONREAL, E_OVERFLOW, E_ARGUMENT, E_DATATYPE, E_INVALID,
       E_WINDOW, E_NOSIGN, E_BOUND, E_TOL, E_MEMORY, E_INCREMENT, E_BADGUESS, E_UNDEFINED, E_COUNT };
extern const char* const ERR_NAME[E_COUNT];

double r14(double x);
int    chk(double x);                     // E_OK, E_OVERFLOW o E_DOMAIN
double fmodp(double a, double b);         // resto positivo
double ipart(double x);
double ang_to_rad(double x);
double rad_to_ang(double r);
double lgam(double x);
double gammp(double a, double x);
double betai(double a, double b, double x);
double normq(double z);                   // P(Z > z)
double invnorm(double p);
double ti_rand(void);
void   ti_seed(double n);
double pow10i(int n);

// ===================================================================== formato
int fmt_mode(double v, char* out, int maxlen, int notation, int fix);
int fmt_num(double v, char* out, int maxlen);
int fmt_frac(double v, char* out, int maxlen);
int fmt_dms(double v, char* out, int maxlen);
int num_tokens(double v, uint8_t* t, int cap);

// ===================================================================== evaluador
struct Stmt { double v; uint8_t conv; bool done; };
int eval_line(const uint8_t* t, int n, Stmt* out, int* errpos);
int eval_expr(const uint8_t* t, int n, double* v, int* errpos);
int eval_fn(int idx, double x, double* y);
int eval_at(const uint8_t* t, int n, int var, double x, double* y);
double integrate(const uint8_t* t, int n, int var, double a, double b, int* err);
double minimize(const uint8_t* t, int n, int var, double a, double b, bool maxi, int* err);

// ===================================================================== editor
extern bool ins_mode;
void ed_clear(Editor& e);
void ed_load(Editor& e, const uint8_t* t, int n);
bool ed_put(Editor& e, uint8_t t);
void ed_del(Editor& e);
void ed_back(Editor& e);
int  ed_text(const Editor& e, char* out, int cap, int* start);   // start[i] = columna del token i
int  ed_draw(const Editor& e, int col, int row, int cont_col, int maxrows, bool cursor, int skip_rows = 0);
int  ed_rows(const Editor& e, int col, int cont_col);
void ed_vert(Editor& e, int dir, int col, int cont_col);
int  key_token(int k, int mod);
void draw_cursor(int col, int row, uint8_t under);

// ===================================================================== pantallas
enum { SC_HOME, SC_ERR, SC_MENU, SC_FORM, SC_YEQ, SC_GRAPH, SC_TABLE, SC_CATALOG, SC_INFO };
enum { MOD_NONE = 0, MOD_2ND = 1, MOD_ALPHA = 2 };
extern int screen;
extern bool blink, second, alpha, alock;

void go(int sc);
void go_home(void);
bool nav_key(int k, int mod);                     // teclas globales (MODE, Y=, MATH...)
void show_error(int code, int ret_screen, int ret_pos);
enum { INFO_STAT, INFO_LIST, INFO_MATRIX, INFO_PRGM, INFO_APPS, INFO_DRAW, INFO_LINK, INFO_STATPLOT,
       INFO_COMPLEX, INFO_SEQ, INFO_ABOUT, INFO_SOLVER, INFO_MEMMGMT, INFO_GDB, INFO_PIC, INFO_STATVARS,
       INFO_STRING, INFO_PARAM, INFO_POLAR, INFO_ZSTAT };
void show_info(int which);
void show_about(void);
void show_solver_info(void);
void show_memmgmt_info(void);
void catalog_paste_open(void);
int  ti_debug_text(char* out, int cap);
Editor* paste_editor(void);                       // editor donde se pegan los menús
void paste_token(uint8_t t);
void home_command(uint8_t t);                     // comandos que se ejecutan en el inicio
void home_message(const char* right_text);

// inicio
void home_key(int k, int mod);
void home_draw(void);
void home_clear(void);
void home_goto(int pos);
void home_clear_entries(void);
extern Editor home_ed;

// menús y formularios
enum { MENU_MATH, MENU_TEST, MENU_ANGLE, MENU_DISTR, MENU_VARS, MENU_MEM, MENU_ZOOM, MENU_CALC,
       MENU_VWIN, MENU_VTBL, MENU_YFUNC, MENU_YONOFF, MENU_RESET, MENU_RALL, MENU_RDEF, MENU_COUNT };
void menu_open(int m);
void menu_key(int k, int mod);
void menu_draw(void);
enum { FORM_MODE, FORM_FORMAT, FORM_WINDOW, FORM_TBLSET, FORM_FACTORS };
void form_open(int f);
void form_key(int k, int mod);
void form_draw(void);
bool form_commit(void);
Editor* form_editor(void);
void catalog_open(void);
void catalog_key(int k, int mod);
void catalog_draw(void);
void info_key(int k, int mod);
void info_draw(void);
void err_key(int k, int mod);
void err_draw(void);

// gráficas
void yeq_key(int k, int mod);
void yeq_draw(void);
Editor* yeq_editor(void);
void yeq_goto(int fn, int pos);
enum { G_PLAIN, G_FREE, G_TRACE };
void graph_open(int mode);
void graph_key(int k, int mod);
void graph_draw(void);
Editor* graph_editor(void);
void graph_calc(int what);       // 1..7 del menú CALC
void graph_zoom(int what);       // acciones del menú ZOOM
void table_open(void);
void graph_reset(void);
void table_key(int k, int mod);
void table_draw(void);
