// svg.h - Dibujos SVG simples hechos por la IA.
#pragma once

// Dibuja el SVG ajustado (sin deformar) al rectángulo x,y,w,h de la pantalla.
// with_text = false para miniaturas (el texto no se lee en tan poco espacio).
void svg_render(const char* src, int n, int x, int y, int w, int h, bool with_text);
// Busca un bloque <svg ...>...</svg> en s. Devuelve 1 y su inicio/fin si lo encuentra.
int svg_find(const char* s, int n, int* start, int* end);
