// keys.h - Las 50 teclas de la TI-83 Plus (más ON), en el orden físico del teclado.
#pragma once
#include <stdint.h>

enum Key {
  K_YEQU, K_WINDOW, K_ZOOM, K_TRACE, K_GRAPH,          // fila F1..F5
  K_2ND, K_MODE, K_DEL, K_LEFT, K_RIGHT, K_UP, K_DOWN,
  K_ALPHA, K_XTTN, K_STAT,
  K_MATH, K_APPS, K_PRGM, K_VARS, K_CLEAR,
  K_INV, K_SIN, K_COS, K_TAN, K_POW,
  K_SQR, K_COMMA, K_LPAREN, K_RPAREN, K_DIV,
  K_LOG, K_7, K_8, K_9, K_MUL,
  K_LN, K_4, K_5, K_6, K_SUB,
  K_STO, K_1, K_2, K_3, K_ADD,
  K_ON, K_0, K_DOT, K_NEG, K_ENTER,
  K_COUNT
};

// Letra verde (ALPHA) impresa sobre cada tecla. 0x88 = theta.
static inline uint8_t alpha_of(int k) {
  switch (k) {
    case K_MATH: return 'A';  case K_APPS: return 'B';  case K_PRGM: return 'C';
    case K_INV: return 'D';   case K_SIN: return 'E';   case K_COS: return 'F';
    case K_TAN: return 'G';   case K_POW: return 'H';   case K_SQR: return 'I';
    case K_COMMA: return 'J'; case K_LPAREN: return 'K'; case K_RPAREN: return 'L';
    case K_DIV: return 'M';   case K_LOG: return 'N';   case K_7: return 'O';
    case K_8: return 'P';     case K_9: return 'Q';     case K_MUL: return 'R';
    case K_LN: return 'S';    case K_4: return 'T';     case K_5: return 'U';
    case K_6: return 'V';     case K_SUB: return 'W';   case K_STO: return 'X';
    case K_1: return 'Y';     case K_2: return 'Z';     case K_3: return 0x88;
    default: return 0;
  }
}
