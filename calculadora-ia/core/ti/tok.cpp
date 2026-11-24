// tok.cpp - Tabla de tokens: lo que muestra cada tecla/función en la pantalla.
#include "ti_int.h"

// Para TK_BIN, arg = nivel de precedencia (1 = or/xor ... 7 = ^).
// Para TK_FUNC, arg = mínimo de argumentos | (máximo << 4).
#define F(a, b) (uint8_t)((a) | ((b) << 4))

static const TokInfo TABLE[T_COUNT] = {
  {"0", TK_NUM, 0}, {"1", TK_NUM, 0}, {"2", TK_NUM, 0}, {"3", TK_NUM, 0}, {"4", TK_NUM, 0},
  {"5", TK_NUM, 0}, {"6", TK_NUM, 0}, {"7", TK_NUM, 0}, {"8", TK_NUM, 0}, {"9", TK_NUM, 0},
  {".", TK_NUM, 0}, {"\x85", TK_NUM, 0}, {"\x86", TK_NEG, 0},
  {"+", TK_BIN, 4}, {"-", TK_BIN, 4}, {"*", TK_BIN, 5}, {"/", TK_BIN, 5}, {"^", TK_BIN, 7},
  {"\x8D\x83", TK_BIN, 7}, {" nPr ", TK_BIN, 6}, {" nCr ", TK_BIN, 6},
  {"=", TK_BIN, 3}, {"\x89", TK_BIN, 3}, {">", TK_BIN, 3}, {"\x8B", TK_BIN, 3}, {"<", TK_BIN, 3},
  {"\x8A", TK_BIN, 3}, {" and ", TK_BIN, 2}, {" or ", TK_BIN, 1}, {" xor ", TK_BIN, 1},
  {"(", TK_LP, 0}, {")", TK_RP, 0}, {",", TK_COMMA, 0}, {":", TK_COLON, 0}, {"\x82", TK_STO, 0},
  {" ", TK_MISC, 0}, {"\"", TK_MISC, 0}, {"?", TK_MISC, 0},
  {"\xB2", TK_POST, 0}, {"\x80", TK_POST, 0}, {"\xB3", TK_POST, 0}, {"!", TK_POST, 0},
  {"\xB0", TK_POST, 0}, {"\x91", TK_POST, 0}, {"'", TK_POST, 0},
  {"\x8C" "Frac", TK_CONV, 0}, {"\x8C" "Dec", TK_CONV, 0}, {"\x8C" "DMS", TK_CONV, 0},
  {"sin(", TK_FUNC, F(1, 1)}, {"cos(", TK_FUNC, F(1, 1)}, {"tan(", TK_FUNC, F(1, 1)},
  {"sin\x80(", TK_FUNC, F(1, 1)}, {"cos\x80(", TK_FUNC, F(1, 1)}, {"tan\x80(", TK_FUNC, F(1, 1)},
  {"sinh(", TK_FUNC, F(1, 1)}, {"cosh(", TK_FUNC, F(1, 1)}, {"tanh(", TK_FUNC, F(1, 1)},
  {"sinh\x80(", TK_FUNC, F(1, 1)}, {"cosh\x80(", TK_FUNC, F(1, 1)}, {"tanh\x80(", TK_FUNC, F(1, 1)},
  {"log(", TK_FUNC, F(1, 1)}, {"ln(", TK_FUNC, F(1, 1)}, {"10^(", TK_FUNC, F(1, 1)},
  {"e^(", TK_FUNC, F(1, 1)}, {"\x83(", TK_FUNC, F(1, 1)}, {"\xB3\x83(", TK_FUNC, F(1, 1)},
  {"abs(", TK_FUNC, F(1, 1)}, {"round(", TK_FUNC, F(1, 2)}, {"iPart(", TK_FUNC, F(1, 1)},
  {"fPart(", TK_FUNC, F(1, 1)}, {"int(", TK_FUNC, F(1, 1)}, {"min(", TK_FUNC, F(2, 2)},
  {"max(", TK_FUNC, F(2, 2)}, {"lcm(", TK_FUNC, F(2, 2)}, {"gcd(", TK_FUNC, F(2, 2)},
  {"fMin(", TK_FUNC, F(4, 5)}, {"fMax(", TK_FUNC, F(4, 5)}, {"nDeriv(", TK_FUNC, F(3, 4)},
  {"fnInt(", TK_FUNC, F(4, 5)},
  {"randInt(", TK_FUNC, F(2, 2)}, {"randNorm(", TK_FUNC, F(2, 2)}, {"randBin(", TK_FUNC, F(2, 2)},
  {"not(", TK_FUNC, F(1, 1)},
  {"R\x8CPr(", TK_FUNC, F(2, 2)}, {"R\x8CP\x88(", TK_FUNC, F(2, 2)},
  {"P\x8CRx(", TK_FUNC, F(2, 2)}, {"P\x8CRy(", TK_FUNC, F(2, 2)},
  {"normalpdf(", TK_FUNC, F(1, 3)}, {"normalcdf(", TK_FUNC, F(2, 4)}, {"invNorm(", TK_FUNC, F(1, 3)},
  {"tpdf(", TK_FUNC, F(2, 2)}, {"tcdf(", TK_FUNC, F(3, 3)},
  {"\x8F\xB2pdf(", TK_FUNC, F(2, 2)}, {"\x8F\xB2" "cdf(", TK_FUNC, F(3, 3)},
  {"Fpdf(", TK_FUNC, F(3, 3)}, {"Fcdf(", TK_FUNC, F(4, 4)},
  {"binompdf(", TK_FUNC, F(3, 3)}, {"binomcdf(", TK_FUNC, F(3, 3)},
  {"poissonpdf(", TK_FUNC, F(2, 2)}, {"poissoncdf(", TK_FUNC, F(2, 2)},
  {"geometpdf(", TK_FUNC, F(2, 2)}, {"geometcdf(", TK_FUNC, F(2, 2)},
  {"\x84", TK_CONST, 0}, {"e", TK_CONST, 0}, {"rand", TK_CONST, 0}, {"Ans", TK_CONST, 0},
  {"A", TK_VAR, 0}, {"B", TK_VAR, 1}, {"C", TK_VAR, 2}, {"D", TK_VAR, 3}, {"E", TK_VAR, 4},
  {"F", TK_VAR, 5}, {"G", TK_VAR, 6}, {"H", TK_VAR, 7}, {"I", TK_VAR, 8}, {"J", TK_VAR, 9},
  {"K", TK_VAR, 10}, {"L", TK_VAR, 11}, {"M", TK_VAR, 12}, {"N", TK_VAR, 13}, {"O", TK_VAR, 14},
  {"P", TK_VAR, 15}, {"Q", TK_VAR, 16}, {"R", TK_VAR, 17}, {"S", TK_VAR, 18}, {"T", TK_VAR, 19},
  {"U", TK_VAR, 20}, {"V", TK_VAR, 21}, {"W", TK_VAR, 22}, {"X", TK_VAR, 23}, {"Y", TK_VAR, 24},
  {"Z", TK_VAR, 25}, {"\x88", TK_VAR, 26},
  {"Y1", TK_YVAR, 0}, {"Y2", TK_YVAR, 1}, {"Y3", TK_YVAR, 2}, {"Y4", TK_YVAR, 3}, {"Y5", TK_YVAR, 4},
  {"Y6", TK_YVAR, 5}, {"Y7", TK_YVAR, 6}, {"Y8", TK_YVAR, 7}, {"Y9", TK_YVAR, 8}, {"Y0", TK_YVAR, 9},
  {"Xmin", TK_SVAR, 0}, {"Xmax", TK_SVAR, 1}, {"Xscl", TK_SVAR, 2}, {"Ymin", TK_SVAR, 3},
  {"Ymax", TK_SVAR, 4}, {"Yscl", TK_SVAR, 5}, {"Xres", TK_SVAR, 6}, {"\x8E" "X", TK_SVAR, 7},
  {"\x8E" "Y", TK_SVAR, 8}, {"XFact", TK_SVAR, 9}, {"YFact", TK_SVAR, 10},
  {"TblStart", TK_SVAR, 11}, {"\x8E" "Tbl", TK_SVAR, 12},
  {"Clear Entries", TK_CMD, 0}, {"FnOn ", TK_CMD, 1}, {"FnOff ", TK_CMD, 2},
};

const TokInfo& tok(uint8_t t) { return TABLE[t < T_COUNT ? t : T_QMARK]; }
int tok_len(uint8_t t) { return s_len(tok(t).text); }
