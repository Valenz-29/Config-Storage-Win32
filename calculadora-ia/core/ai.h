// ai.h - Modo IA: chat con Claude, cámara y WiFi.
#pragma once
#include <stdint.h>

#define IO_CAP 32768
extern char g_io[IO_CAP];   // buffer de intercambio con la plataforma (UTF-8)

// Instrucciones para el agente. Las usan el simulador y el firmware.
#define AI_SYSTEM_PROMPT \
  "Eres un asistente dentro de una calculadora TI-83 Plus modificada, con una pantalla " \
  "de 53x16 caracteres. Responde en el idioma del usuario, en texto plano y breve " \
  "(idealmente menos de 120 palabras). No uses markdown, tablas, emojis ni LaTeX: " \
  "escribe las fórmulas en una línea, como 3x^2+2x o √(x+1). Si te mandan una foto " \
  "de un ejercicio, explica los pasos de forma clara y corta."

void ai_enter(void);
void ai_exit(void);
void ai_key(int key);
void ai_char(uint8_t ch);          // carácter escrito con el teclado del PC (solo simulador)
bool ai_wants_text(void);
void ai_tick(uint32_t now);
void ai_draw(void);
void ai_on_reply(int len, int ok);
void ai_set_assistant(const char* utf8, int len, uint16_t color);
void ai_new_chat(void);
void ai_on_photo(int ok);
void ai_on_wifi_scan(int len);
void ai_on_wifi_result(int ok);
