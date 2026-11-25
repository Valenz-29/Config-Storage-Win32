// menus.cpp - Menús con pestañas (MATH, TEST, ANGLE, DISTR, VARS, MEM, ZOOM, CALC),
// pantallas de opciones (MODE, FORMAT, WINDOW, TBLSET) y CATALOG.
#include "ti_int.h"

// ===================================================================== menús
enum { A_PASTE, A_SUB, A_ACT, A_INFO, A_PASTE_HOME };
enum { ACT_QUIT, ACT_ABOUT, ACT_RESET_ALL, ACT_RESET_DEF, ACT_SOLVER, ACT_MEMMGMT,
       ACT_CALC = 20, ACT_ZOOM = 40 };
struct MItem { const char* label; uint8_t type, arg; };
struct MTab { const char* title; const MItem* items; uint8_t n; };
struct MenuDef { const MTab* tabs; uint8_t ntabs; };

#define N(a) (uint8_t)(sizeof(a) / sizeof(a[0]))
#define TK(t) { 0, A_PASTE, t }

// Menús que llegan con el emulador: se ven igual que en la TI y cada opción explica que falta.
#define IN(l, w) { l, A_INFO, w }
static const MItem M_CPX[] = { IN("conj(", INFO_COMPLEX), IN("real(", INFO_COMPLEX), IN("imag(", INFO_COMPLEX),
  IN("angle(", INFO_COMPLEX), IN("abs(", INFO_COMPLEX), IN("\x8CRect", INFO_COMPLEX), IN("\x8CPolar", INFO_COMPLEX) };
static const MItem M_DDRAW[] = { IN("ShadeNorm(", INFO_DRAW), IN("Shade_t(", INFO_DRAW), IN("Shade\x8F\xB2(", INFO_DRAW),
  IN("ShadeF(", INFO_DRAW) };
static const MItem M_SEDIT[] = { IN("Edit...", INFO_STAT), IN("SortA(", INFO_STAT), IN("SortD(", INFO_STAT),
  IN("ClrList", INFO_STAT), IN("SetUpEditor", INFO_STAT) };
static const MItem M_SCALC[] = { IN("1-Var Stats", INFO_STAT), IN("2-Var Stats", INFO_STAT), IN("Med-Med", INFO_STAT),
  IN("LinReg(ax+b)", INFO_STAT), IN("QuadReg", INFO_STAT), IN("CubicReg", INFO_STAT), IN("QuartReg", INFO_STAT),
  IN("LinReg(a+bx)", INFO_STAT), IN("LnReg", INFO_STAT), IN("ExpReg", INFO_STAT), IN("PwrReg", INFO_STAT),
  IN("Logistic", INFO_STAT), IN("SinReg", INFO_STAT) };
static const MItem M_STESTS[] = { IN("Z-Test...", INFO_STAT), IN("T-Test...", INFO_STAT), IN("2-SampZTest...", INFO_STAT),
  IN("2-SampTTest...", INFO_STAT), IN("1-PropZTest...", INFO_STAT), IN("2-PropZTest...", INFO_STAT),
  IN("ZInterval...", INFO_STAT), IN("TInterval...", INFO_STAT), IN("2-SampZInt...", INFO_STAT),
  IN("2-SampTInt...", INFO_STAT), IN("1-PropZInt...", INFO_STAT), IN("2-PropZInt...", INFO_STAT),
  IN("\x8F\xB2-Test...", INFO_STAT), IN("2-SampFTest...", INFO_STAT), IN("LinRegTTest...", INFO_STAT), IN("ANOVA(", INFO_STAT) };
static const MTab T_STATM[] = { { "EDIT", M_SEDIT, N(M_SEDIT) }, { "CALC", M_SCALC, N(M_SCALC) }, { "TESTS", M_STESTS, N(M_STESTS) } };
static const MItem M_LNAMES[] = { IN("L1", INFO_LIST), IN("L2", INFO_LIST), IN("L3", INFO_LIST), IN("L4", INFO_LIST),
  IN("L5", INFO_LIST), IN("L6", INFO_LIST) };
static const MItem M_LOPS[] = { IN("SortA(", INFO_LIST), IN("SortD(", INFO_LIST), IN("dim(", INFO_LIST), IN("Fill(", INFO_LIST),
  IN("seq(", INFO_LIST), IN("cumSum(", INFO_LIST), IN("\x8EList(", INFO_LIST), IN("Select(", INFO_LIST),
  IN("augment(", INFO_LIST), IN("List\x8Cmatr(", INFO_LIST), IN("Matr\x8Clist(", INFO_LIST), IN("L", INFO_LIST) };
static const MItem M_LMATH[] = { IN("min(", INFO_LIST), IN("max(", INFO_LIST), IN("mean(", INFO_LIST), IN("median(", INFO_LIST),
  IN("sum(", INFO_LIST), IN("prod(", INFO_LIST), IN("stdDev(", INFO_LIST), IN("variance(", INFO_LIST) };
static const MTab T_LISTM[] = { { "NAMES", M_LNAMES, N(M_LNAMES) }, { "OPS", M_LOPS, N(M_LOPS) }, { "MATH", M_LMATH, N(M_LMATH) } };
static const MItem M_MNAMES[] = { IN("[A]", INFO_MATRIX), IN("[B]", INFO_MATRIX), IN("[C]", INFO_MATRIX), IN("[D]", INFO_MATRIX),
  IN("[E]", INFO_MATRIX), IN("[F]", INFO_MATRIX), IN("[G]", INFO_MATRIX), IN("[H]", INFO_MATRIX), IN("[I]", INFO_MATRIX),
  IN("[J]", INFO_MATRIX) };
static const MItem M_MMATH[] = { IN("det(", INFO_MATRIX), IN("T", INFO_MATRIX), IN("dim(", INFO_MATRIX), IN("Fill(", INFO_MATRIX),
  IN("identity(", INFO_MATRIX), IN("randM(", INFO_MATRIX), IN("augment(", INFO_MATRIX), IN("Matr\x8Clist(", INFO_MATRIX),
  IN("List\x8Cmatr(", INFO_MATRIX), IN("cumSum(", INFO_MATRIX), IN("ref(", INFO_MATRIX), IN("rref(", INFO_MATRIX),
  IN("rowSwap(", INFO_MATRIX), IN("row+(", INFO_MATRIX), IN("*row(", INFO_MATRIX), IN("*row+(", INFO_MATRIX) };
static const MTab T_MATRXM[] = { { "NAMES", M_MNAMES, N(M_MNAMES) }, { "MATH", M_MMATH, N(M_MMATH) }, { "EDIT", M_MNAMES, N(M_MNAMES) } };
static const MItem M_PNEW[] = { IN("Create New", INFO_PRGM) };
static const MTab T_PRGMM[] = { { "EXEC", 0, 0 }, { "EDIT", 0, 0 }, { "NEW", M_PNEW, 1 } };
static const MItem M_DRAW[] = { IN("ClrDraw", INFO_DRAW), IN("Line(", INFO_DRAW), IN("Horizontal", INFO_DRAW),
  IN("Vertical", INFO_DRAW), IN("Tangent(", INFO_DRAW), IN("DrawF", INFO_DRAW), IN("Shade(", INFO_DRAW),
  IN("DrawInv", INFO_DRAW), IN("Circle(", INFO_DRAW), IN("Text(", INFO_DRAW), IN("Pen", INFO_DRAW) };
static const MItem M_POINTS[] = { IN("Pt-On(", INFO_DRAW), IN("Pt-Off(", INFO_DRAW), IN("Pt-Change(", INFO_DRAW),
  IN("Pxl-On(", INFO_DRAW), IN("Pxl-Off(", INFO_DRAW), IN("Pxl-Change(", INFO_DRAW), IN("pxl-Test(", INFO_DRAW) };
static const MItem M_DSTO[] = { IN("StorePic", INFO_DRAW), IN("RecallPic", INFO_DRAW), IN("StoreGDB", INFO_DRAW),
  IN("RecallGDB", INFO_DRAW) };
static const MTab T_DRAWM[] = { { "DRAW", M_DRAW, N(M_DRAW) }, { "POINTS", M_POINTS, N(M_POINTS) }, { "STO", M_DSTO, N(M_DSTO) } };
static const MItem M_APPS[] = { IN("Finance...", INFO_APPS) };
static const MTab T_APPSM[] = { { "APPLICATIONS", M_APPS, 1 } };

static const MItem M_MATH[] = { TK(T_FRAC), TK(T_DEC), TK(T_CUBE), TK(T_CBRT), TK(T_XROOT), TK(T_FMIN),
  TK(T_FMAX), TK(T_NDERIV), TK(T_FNINT), { "Solver...", A_INFO, INFO_SOLVER } };
static const MItem M_NUM[] = { TK(T_ABS), TK(T_ROUND), TK(T_IPART), TK(T_FPART), TK(T_INT), TK(T_MINF),
  TK(T_MAXF), TK(T_LCM), TK(T_GCD) };
static const MItem M_PRB[] = { TK(T_RAND), { "nPr", A_PASTE, T_NPR }, { "nCr", A_PASTE, T_NCR }, TK(T_FACT),
  TK(T_RANDINT), TK(T_RANDNORM), TK(T_RANDBIN) };
static const MTab T_MATHM[] = { { "MATH", M_MATH, N(M_MATH) }, { "NUM", M_NUM, N(M_NUM) }, { "CPX", M_CPX, N(M_CPX) }, { "PRB", M_PRB, N(M_PRB) } };

static const MItem M_TEST[] = { TK(T_EQ), TK(T_NE), TK(T_GT), TK(T_GE), TK(T_LT), TK(T_LE) };
static const MItem M_LOGIC[] = { { "and", A_PASTE, T_AND }, { "or", A_PASTE, T_OR }, { "xor", A_PASTE, T_XOR }, TK(T_NOT) };
static const MTab T_TESTM[] = { { "TEST", M_TEST, N(M_TEST) }, { "LOGIC", M_LOGIC, N(M_LOGIC) } };

static const MItem M_ANGLE[] = { TK(T_DEGS), TK(T_MINS), TK(T_RADS), TK(T_DMS), TK(T_RPR), TK(T_RPT), TK(T_PRX), TK(T_PRY) };
static const MTab T_ANGLEM[] = { { "ANGLE", M_ANGLE, N(M_ANGLE) } };

static const MItem M_DISTR[] = { TK(T_NORMPDF), TK(T_NORMCDF), TK(T_INVNORM), TK(T_TPDF), TK(T_TCDF), TK(T_CHIPDF),
  TK(T_CHICDF), TK(T_FPDF), TK(T_FCDF), TK(T_BINPDF), TK(T_BINCDF), TK(T_POIPDF), TK(T_POICDF), TK(T_GEOPDF), TK(T_GEOCDF) };
static const MTab T_DISTRM[] = { { "DISTR", M_DISTR, N(M_DISTR) }, { "DRAW", M_DDRAW, N(M_DDRAW) } };

static const MItem M_VARS[] = { { "Window...", A_SUB, MENU_VWIN }, { "Zoom...", A_INFO, INFO_GDB },
  { "GDB...", A_INFO, INFO_GDB }, { "Picture...", A_INFO, INFO_PIC }, { "Statistics...", A_INFO, INFO_STATVARS },
  { "Table...", A_SUB, MENU_VTBL }, { "String...", A_INFO, INFO_STRING } };
static const MItem M_YVARS[] = { { "Function...", A_SUB, MENU_YFUNC }, { "Parametric...", A_INFO, INFO_PARAM },
  { "Polar...", A_INFO, INFO_POLAR }, { "On/Off...", A_SUB, MENU_YONOFF } };
static const MTab T_VARSM[] = { { "VARS", M_VARS, N(M_VARS) }, { "Y-VARS", M_YVARS, N(M_YVARS) } };

static const MItem M_VWIN[] = { TK(T_XMIN), TK(T_XMAX), TK(T_XSCL), TK(T_YMIN), TK(T_YMAX), TK(T_YSCL),
  TK(T_XRES), TK(T_DX), TK(T_DY), TK(T_XFACT), TK(T_YFACT) };
static const MTab T_VWINM[] = { { "X/Y", M_VWIN, N(M_VWIN) } };
static const MItem M_VTBL[] = { TK(T_TBLSTART), TK(T_DTBL) };
static const MTab T_VTBLM[] = { { "TABLE", M_VTBL, N(M_VTBL) } };
static const MItem M_YFUNC[] = { TK(T_Y1), TK(T_Y1 + 1), TK(T_Y1 + 2), TK(T_Y1 + 3), TK(T_Y1 + 4), TK(T_Y1 + 5),
  TK(T_Y1 + 6), TK(T_Y1 + 7), TK(T_Y1 + 8), TK(T_Y0) };
static const MTab T_YFUNCM[] = { { "FUNCTION", M_YFUNC, N(M_YFUNC) } };
static const MItem M_YONOFF[] = { { "FnOn", A_PASTE, T_FNON }, { "FnOff", A_PASTE, T_FNOFF } };
static const MTab T_YONOFFM[] = { { "ON/OFF", M_YONOFF, N(M_YONOFF) } };

static const MItem M_MEM[] = { { "About", A_ACT, ACT_ABOUT }, { "Mem Mgmt/Del...", A_ACT, ACT_MEMMGMT },
  { "Clear Entries", A_PASTE_HOME, T_CLRENT }, { "ClrAllLists", A_INFO, INFO_LIST }, { "Archive", A_ACT, ACT_MEMMGMT },
  { "UnArchive", A_ACT, ACT_MEMMGMT }, { "Reset...", A_SUB, MENU_RESET }, { "Group...", A_ACT, ACT_MEMMGMT } };
static const MTab T_MEMM[] = { { "MEMORY", M_MEM, N(M_MEM) } };
static const MItem M_RESET[] = { { "All RAM...", A_SUB, MENU_RALL }, { "Defaults...", A_SUB, MENU_RDEF } };
static const MTab T_RESETM[] = { { "RAM", M_RESET, N(M_RESET) } };
static const MItem M_RALL[] = { { "No", A_ACT, ACT_QUIT }, { "Reset", A_ACT, ACT_RESET_ALL } };
static const MTab T_RALLM[] = { { "RESET RAM", M_RALL, N(M_RALL) } };
static const MItem M_RDEF[] = { { "No", A_ACT, ACT_QUIT }, { "Reset", A_ACT, ACT_RESET_DEF } };
static const MTab T_RDEFM[] = { { "RESET DEFAULTS", M_RDEF, N(M_RDEF) } };

static const MItem M_ZOOM[] = { { "ZBox", A_ACT, ACT_ZOOM + 1 }, { "Zoom In", A_ACT, ACT_ZOOM + 2 },
  { "Zoom Out", A_ACT, ACT_ZOOM + 3 }, { "ZDecimal", A_ACT, ACT_ZOOM + 4 }, { "ZSquare", A_ACT, ACT_ZOOM + 5 },
  { "ZStandard", A_ACT, ACT_ZOOM + 6 }, { "ZTrig", A_ACT, ACT_ZOOM + 7 }, { "ZInteger", A_ACT, ACT_ZOOM + 8 },
  { "ZoomStat", A_INFO, INFO_ZSTAT }, { "ZoomFit", A_ACT, ACT_ZOOM + 10 } };
static const MItem M_ZMEM[] = { { "ZPrevious", A_ACT, ACT_ZOOM + 11 }, { "ZoomSto", A_ACT, ACT_ZOOM + 12 },
  { "ZoomRcl", A_ACT, ACT_ZOOM + 13 }, { "SetFactors...", A_ACT, ACT_ZOOM + 14 } };
static const MTab T_ZOOMM[] = { { "ZOOM", M_ZOOM, N(M_ZOOM) }, { "MEMORY", M_ZMEM, N(M_ZMEM) } };

static const MItem M_CALC[] = { { "value", A_ACT, ACT_CALC + 1 }, { "zero", A_ACT, ACT_CALC + 2 },
  { "minimum", A_ACT, ACT_CALC + 3 }, { "maximum", A_ACT, ACT_CALC + 4 }, { "intersect", A_ACT, ACT_CALC + 5 },
  { "dy/dx", A_ACT, ACT_CALC + 6 }, { "\x90" "f(x)dx", A_ACT, ACT_CALC + 7 } };
static const MTab T_CALCM[] = { { "CALCULATE", M_CALC, N(M_CALC) } };


static const MenuDef MENUS[MENU_COUNT] = {
  { T_MATHM, N(T_MATHM) }, { T_TESTM, N(T_TESTM) }, { T_ANGLEM, 1 }, { T_DISTRM, 2 }, { T_VARSM, N(T_VARSM) },
  { T_MEMM, 1 }, { T_ZOOMM, N(T_ZOOMM) }, { T_CALCM, 1 }, { T_VWINM, 1 }, { T_VTBLM, 1 }, { T_YFUNCM, 1 },
  { T_YONOFFM, 1 }, { T_RESETM, 1 }, { T_RALLM, 1 }, { T_RDEFM, 1 },
  { T_STATM, 3 }, { T_LISTM, 3 }, { T_MATRXM, 3 }, { T_PRGMM, 3 }, { T_DRAWM, 3 }, { T_APPSM, 1 },
};

static int mcur = 0, mtab = 0, mitem = 0, mtop = 0, mret = SC_HOME;

void menu_open(int m) {
  if (screen != SC_MENU) mret = screen == SC_ERR || screen == SC_INFO || screen == SC_CATALOG ? SC_HOME : screen;
  mcur = m; mtab = 0; mitem = 0; mtop = 0;
  ins_mode = false;
  go(SC_MENU);
}

static const char* item_label(const MItem& it) { return it.label ? it.label : tok(it.arg).text; }

static void menu_select(const MItem& it) {
  switch (it.type) {
    case A_PASTE: paste_token(it.arg); return;
    case A_PASTE_HOME: ed_put(home_ed, it.arg); go(SC_HOME); return;
    case A_SUB: { int keep = mret; menu_open(it.arg); mret = keep; return; }
    case A_INFO: show_info(it.arg); return;
    case A_ACT:
      if (it.arg >= ACT_ZOOM) { graph_zoom(it.arg - ACT_ZOOM); return; }
      if (it.arg >= ACT_CALC) { graph_calc(it.arg - ACT_CALC); return; }
      switch (it.arg) {
        case ACT_QUIT: go(SC_HOME); return;
        case ACT_ABOUT: show_about(); return;
        case ACT_MEMMGMT: show_memmgmt_info(); return;
        case ACT_RESET_ALL:
          state_defaults(true); home_clear(); home_clear_entries(); ed_clear(home_ed);
          home_message("RAM cleared"); go(SC_HOME); return;
        case ACT_RESET_DEF:
          state_defaults(false); home_message("Defaults set"); go(SC_HOME); return;
      }
  }
}

void menu_key(int k, int mod) {
  const MenuDef& m = MENUS[mcur];
  const MTab& tb = m.tabs[mtab];
  if (mod == MOD_NONE) {
    switch (k) {
      case K_LEFT: mtab = (mtab + m.ntabs - 1) % m.ntabs; mitem = 0; mtop = 0; return;
      case K_RIGHT: mtab = (mtab + 1) % m.ntabs; mitem = 0; mtop = 0; return;
      case K_UP: if (tb.n) mitem = (mitem + tb.n - 1) % tb.n; break;
      case K_DOWN: if (tb.n) mitem = (mitem + 1) % tb.n; break;
      case K_ENTER: if (tb.n) menu_select(tb.items[mitem]); return;
      case K_CLEAR: go(mret == SC_MENU ? SC_HOME : mret); return;
    }
    if (k == K_UP || k == K_DOWN) {
      if (mitem < mtop) mtop = mitem;
      if (mitem >= mtop + 7) mtop = mitem - 6;
      return;
    }
  }
  int idx = -1;
  if (mod == MOD_NONE) {
    int t = key_token(k, MOD_NONE);
    if (t >= 0 && t <= 9) idx = t == 0 ? 9 : t - 1;
  } else if (mod == MOD_ALPHA) {
    uint8_t a = alpha_of(k);
    if (a >= 'A' && a <= 'Z') idx = 10 + (a - 'A');
  }
  if (idx >= 0) { if (idx < tb.n) menu_select(tb.items[idx]); return; }
  nav_key(k, mod);
}

void menu_draw(void) {
  lcd_clear();
  const MenuDef& m = MENUS[mcur];
  int col = 0;
  for (int t = 0; t < m.ntabs; t++) {
    const char* s = m.tabs[t].title;
    lcd_str(col, 0, s, t == mtab);
    col += s_len(s) + 1;
  }
  const MTab& tb = m.tabs[mtab];
  for (int r = 0; r < 7 && mtop + r < tb.n; r++) {
    int i = mtop + r;
    char num = i < 9 ? (char)('1' + i) : i == 9 ? '0' : (char)('A' + i - 10);
    char sep = ':';
    if (r == 6 && mtop + 7 < tb.n) sep = (char)CH_DOWN;
    if (r == 0 && mtop > 0) sep = (char)CH_UP;
    char pre[2] = { num, sep };
    lcd_text(0, r + 1, pre, 2, i == mitem);
    lcd_text(2, r + 1, item_label(tb.items[i]), s_len(item_label(tb.items[i])) > 14 ? 14 : s_len(item_label(tb.items[i])));
  }
}

// ===================================================================== formularios
enum { R_TITLE, R_CHOICE, R_VALUE };
struct FRow { uint8_t type; const char* label; const char* const* opts; uint8_t nopts, sep, id; };

static const char* const O_NOT[] = { "Normal", "Sci", "Eng" };
static const char* const O_FIX[] = { "Float", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
static const char* const O_ANG[] = { "Radian", "Degree" };
static const char* const O_FUNC[] = { "Func", "Par", "Pol", "Seq" };
static const char* const O_DOT[] = { "Connected", "Dot" };
static const char* const O_SIM[] = { "Sequential", "Simul" };
static const char* const O_REAL[] = { "Real", "a+bi", "re^\x88i" };
static const char* const O_FULL[] = { "Full", "Horiz", "G-T" };
static const char* const O_RECT[] = { "RectGC", "PolarGC" };
static const char* const O_COORD[] = { "CoordOn", "CoordOff" };
static const char* const O_GRID[] = { "GridOff", "GridOn" };
static const char* const O_AXES[] = { "AxesOn", "AxesOff" };
static const char* const O_LABEL[] = { "LabelOff", "LabelOn" };
static const char* const O_EXPR[] = { "ExprOn", "ExprOff" };
static const char* const O_AUTO[] = { "Auto" };

static const FRow F_MODE[] = {
  { R_CHOICE, 0, O_NOT, 3, 1, 0 }, { R_CHOICE, 0, O_FIX, 11, 0, 1 }, { R_CHOICE, 0, O_ANG, 2, 1, 2 },
  { R_CHOICE, 0, O_FUNC, 4, 1, 3 }, { R_CHOICE, 0, O_DOT, 2, 1, 4 }, { R_CHOICE, 0, O_SIM, 2, 1, 5 },
  { R_CHOICE, 0, O_REAL, 3, 1, 6 }, { R_CHOICE, 0, O_FULL, 3, 1, 7 } };
static const FRow F_FORMAT[] = {
  { R_CHOICE, 0, O_RECT, 2, 1, 10 }, { R_CHOICE, 0, O_COORD, 2, 1, 11 }, { R_CHOICE, 0, O_GRID, 2, 1, 12 },
  { R_CHOICE, 0, O_AXES, 2, 1, 13 }, { R_CHOICE, 0, O_LABEL, 2, 1, 14 }, { R_CHOICE, 0, O_EXPR, 2, 1, 15 } };
static const FRow F_WINDOW[] = {
  { R_TITLE, "WINDOW", 0, 0, 0, 0 }, { R_VALUE, "Xmin", 0, 0, 0, 30 }, { R_VALUE, "Xmax", 0, 0, 0, 31 },
  { R_VALUE, "Xscl", 0, 0, 0, 32 }, { R_VALUE, "Ymin", 0, 0, 0, 33 }, { R_VALUE, "Ymax", 0, 0, 0, 34 },
  { R_VALUE, "Yscl", 0, 0, 0, 35 }, { R_VALUE, "Xres", 0, 0, 0, 36 } };
static const FRow F_TBLSET[] = {
  { R_TITLE, "TABLE SETUP", 0, 0, 0, 0 }, { R_VALUE, " TblStart", 0, 0, 0, 40 }, { R_VALUE, " \x8ETbl", 0, 0, 0, 41 },
  { R_CHOICE, "Indpnt:", O_AUTO, 1, 1, 20 }, { R_CHOICE, "Depend:", O_AUTO, 1, 1, 21 } };
static const FRow F_FACT[] = {
  { R_TITLE, "ZOOM FACTORS", 0, 0, 0, 0 }, { R_VALUE, "XFact", 0, 0, 0, 50 }, { R_VALUE, "YFact", 0, 0, 0, 51 } };

static const FRow* frows = F_MODE;
static int fcount = 0, frow = 0, fopt = 0, fwhich = 0;
static Editor fed;

// Opciones que se ven pero aún no se pueden elegir (Par, a+bi, Horiz, PolarGC...).
static int selectable(const FRow& r) { return r.id == 3 || r.id == 6 || r.id == 7 || r.id == 10 ? 1 : r.nopts; }

static int choice_get(int id) {
  switch (id) {
    case 0: return S.notation; case 1: return S.fix + 1; case 2: return S.degree; case 4: return S.dotmode;
    case 5: return S.simul; case 11: return !S.coord; case 12: return S.grid; case 13: return !S.axes;
    case 14: return S.label; case 15: return !S.expr;
  }
  return 0;
}
static void choice_set(int id, int v) {
  switch (id) {
    case 0: S.notation = (uint8_t)v; break; case 1: S.fix = (int8_t)(v - 1); break;
    case 2: S.degree = (uint8_t)v; break;   case 4: S.dotmode = (uint8_t)v; break;
    case 5: S.simul = (uint8_t)v; break;    case 11: S.coord = !v; break;
    case 12: S.grid = (uint8_t)v; break;    case 13: S.axes = !v; break;
    case 14: S.label = (uint8_t)v; break;   case 15: S.expr = !v; break;
  }
  graph_dirty = true;
}
static double* value_ptr(int id) {
  switch (id) {
    case 30: return &S.xmin; case 31: return &S.xmax; case 32: return &S.xscl; case 33: return &S.ymin;
    case 34: return &S.ymax; case 35: return &S.yscl; case 36: return &S.xres; case 40: return &S.tblstart;
    case 41: return &S.dtbl; case 50: return &S.xfact; case 51: return &S.yfact;
  }
  return 0;
}

static void load_row(void) {
  const FRow& r = frows[frow];
  if (r.type == R_VALUE) {
    uint8_t t[24]; int n = num_tokens(*value_ptr(r.id), t, 24);
    ed_load(fed, t, n); fed.cur = 0; fed.fresh = true;
  } else fopt = choice_get(r.id);
}

void form_open(int f) {
  fwhich = f;
  switch (f) {
    case FORM_MODE: frows = F_MODE; fcount = N(F_MODE); break;
    case FORM_FORMAT: frows = F_FORMAT; fcount = N(F_FORMAT); break;
    case FORM_WINDOW: frows = F_WINDOW; fcount = N(F_WINDOW); break;
    case FORM_TBLSET: frows = F_TBLSET; fcount = N(F_TBLSET); break;
    default: frows = F_FACT; fcount = N(F_FACT); break;
  }
  frow = frows[0].type == R_TITLE ? 1 : 0;
  ins_mode = false;
  load_row();
  go(SC_FORM);
}

Editor* form_editor(void) { return frows[frow].type == R_VALUE ? &fed : 0; }

// Guarda el valor que se está editando. Devuelve false si hubo error (y lo muestra).
bool form_commit(void) {
  const FRow& r = frows[frow];
  if (r.type != R_VALUE) return true;
  if (fed.fresh || fed.n == 0) return true;
  double v; int pos = 0;
  int e = eval_expr(fed.t, fed.n, &v, &pos);
  if (!e) {
    if (r.id == 36 && (v != m_floor(v) || v < 1 || v > 8)) e = E_DOMAIN;
    else if (r.id == 41 && v == 0) e = E_INCREMENT;
    else if ((r.id == 50 || r.id == 51) && (v < 1 || v > 100)) e = E_DOMAIN;
  }
  if (e) { fed.cur = pos > fed.n ? fed.n : pos; fed.fresh = false; show_error(e, SC_FORM, 0); return false; }
  *value_ptr(r.id) = v;
  graph_dirty = true;
  load_row();
  return true;
}

static void move_row(int dir) {
  if (!form_commit()) return;
  int r = frow;
  for (int i = 0; i < fcount; i++) {
    r = (r + dir + fcount) % fcount;
    if (frows[r].type != R_TITLE) break;
  }
  frow = r; ins_mode = false;
  load_row();
}

void form_key(int k, int mod) {
  const FRow& r = frows[frow];
  if (mod == MOD_NONE && (k == K_UP || k == K_DOWN)) { move_row(k == K_UP ? -1 : 1); return; }
  if (r.type == R_CHOICE) {
    if (mod == MOD_NONE) {
      if (k == K_LEFT) { if (fopt > 0) fopt--; return; }
      if (k == K_RIGHT) { if (fopt < selectable(r) - 1) fopt++; return; }
      if (k == K_ENTER) { choice_set(r.id, fopt); return; }
      if (k == K_CLEAR) { go_home(); return; }
    }
    nav_key(k, mod);
    return;
  }
  // fila con valor
  if (mod == MOD_2ND && k == K_DEL) { ins_mode = true; fed.fresh = false; return; }
  if (mod == MOD_NONE) {
    switch (k) {
      case K_ENTER: move_row(1); return;
      case K_CLEAR: ed_clear(fed); return;
      case K_DEL: ed_del(fed); return;
      case K_LEFT: if (fed.cur > 0) fed.cur--; fed.fresh = false; ins_mode = false; return;
      case K_RIGHT: if (fed.cur < fed.n) fed.cur++; fed.fresh = false; ins_mode = false; return;
    }
  }
  int t = key_token(k, mod);
  if (t >= 0) { ed_put(fed, (uint8_t)t); return; }
  nav_key(k, mod);
}

void form_draw(void) {
  lcd_clear();
  for (int i = 0; i < fcount && i < 8; i++) {
    const FRow& r = frows[i];
    if (r.type == R_TITLE) { lcd_str(0, i, r.label); continue; }
    if (r.type == R_VALUE) {
      int col = s_len(r.label);
      lcd_str(0, i, r.label);
      lcd_char(col, i, '=');
      if (i == frow) ed_draw(fed, col + 1, i, col + 1, 1, true);
      else { char b[20]; int n = fmt_num(*value_ptr(r.id), b, 15 - col); lcd_text(col + 1, i, b, n); }
      continue;
    }
    int col = 0;
    if (r.label) { lcd_str(0, i, r.label); col = s_len(r.label) + 1; }
    int sel = choice_get(r.id);
    for (int o = 0; o < r.nopts; o++) {
      const char* s = r.opts[o];
      bool inv = o == sel;
      if (i == frow && o == fopt && blink) inv = !inv;
      lcd_str(col, i, s, inv);
      col += s_len(s) + (r.sep || (r.id == 1 && o == 0) ? 1 : 0);
    }
  }
}

// ===================================================================== CATALOG
static uint8_t cat[160];
static int ncat = 0, csel = 0, ctop = 0;

static int cat_key(uint8_t t) {     // letra para ordenar (los símbolos van al final)
  const char* s = tok(t).text;
  while (*s == ' ') s++;
  uint8_t c = (uint8_t)*s;
  if (c >= 'a' && c <= 'z') c = (uint8_t)(c - 32);
  return c >= 'A' && c <= 'Z' ? c : 200 + c % 50;
}
static int cat_cmp(uint8_t a, uint8_t b) {
  const char* x = tok(a).text; const char* y = tok(b).text;
  while (*x == ' ') x++;
  while (*y == ' ') y++;
  int ka = cat_key(a), kb = cat_key(b);
  if (ka != kb) return ka - kb;
  for (;; x++, y++) {
    int cx = (uint8_t)*x, cy = (uint8_t)*y;
    if (cx >= 'A' && cx <= 'Z') cx += 32;
    if (cy >= 'A' && cy <= 'Z') cy += 32;
    if (cx != cy || !cx) return cx - cy;
  }
}

void catalog_open(void) {
  if (!ncat) {
    for (int t = 0; t < T_COUNT; t++) {
      int k = tok((uint8_t)t).kind;
      if (k == TK_FUNC || k == TK_CONV || k == TK_CMD || t == T_NPR || t == T_NCR || t == T_AND ||
          t == T_OR || t == T_XOR || t == T_RAND || t == T_FACT)
        cat[ncat++] = (uint8_t)t;
    }
    for (int i = 1; i < ncat; i++)
      for (int j = i; j > 0 && cat_cmp(cat[j - 1], cat[j]) > 0; j--) { uint8_t x = cat[j]; cat[j] = cat[j - 1]; cat[j - 1] = x; }
  }
  csel = 0; ctop = 0;
  go(SC_CATALOG);
}

void catalog_key(int k, int mod) {
  if (mod == MOD_NONE) {
    switch (k) {
      case K_UP: if (csel > 0) csel--; if (csel < ctop) ctop = csel; return;
      case K_DOWN: if (csel < ncat - 1) csel++; if (csel >= ctop + 7) ctop = csel - 6; return;
      case K_ENTER: paste_token(cat[csel]); return;
      case K_CLEAR: go(SC_HOME); return;
    }
  }
  uint8_t a = alpha_of(k);                   // el CATALOG está en A-LOCK: las letras saltan
  if (a >= 'A' && a <= 'Z' && mod != MOD_2ND) {
    for (int i = 0; i < ncat; i++) if (cat_key(cat[i]) == a) { csel = i; ctop = i; if (ctop > ncat - 7) ctop = ncat - 7 > 0 ? ncat - 7 : 0; return; }
    return;
  }
  nav_key(k, mod);
}

void catalog_draw(void) {
  lcd_clear();
  lcd_str(0, 0, "CATALOG");
  for (int r = 0; r < 7 && ctop + r < ncat; r++) {
    int i = ctop + r;
    if (i == csel) lcd_char(0, r + 1, CH_TRI);
    const char* s = tok(cat[i]).text;
    while (*s == ' ') s++;
    int n = s_len(s); while (n && s[n - 1] == ' ') n--;
    lcd_text(1, r + 1, s, n > 15 ? 15 : n);
  }
}
