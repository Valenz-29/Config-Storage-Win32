// ti.h - Modo normal: pantalla de inicio estilo TI-83 Plus.
// Es una calculadora provisoria hecha por nosotros. Más adelante se reemplaza
// por el emulador con la ROM de tu propia calculadora, sin cambiar el resto.
#pragma once
#include <stdint.h>

void ti_init(void);
void ti_key(int key);
void ti_tick(uint32_t now);
void ti_draw(void);
bool ti_is_on(void);
// Gráfica pedida por la IA: carga funciones en Y1.. y abre GRAPH (-1 = bien, si no el índice que falló)
int ti_load_graph(const char* const* fns, const int* lens, int n, const double* win);
