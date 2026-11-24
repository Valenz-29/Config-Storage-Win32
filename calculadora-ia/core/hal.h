// hal.h - Lo que cada plataforma (simulador web o XIAO ESP32S3) debe implementar.
// El núcleo (core/) no sabe si corre en el navegador o en la calculadora real:
// todo lo que depende del hardware pasa por estas funciones.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t hal_millis(void);

// --- Cámara ---------------------------------------------------------------
void hal_camera_start(void);
void hal_camera_stop(void);
// Dibuja el cuadro actual de la cámara en el framebuffer (RGB565).
// Devuelve 1 si dibujó un cuadro nuevo.
int  hal_camera_preview(uint16_t* fb, int stride, int x, int y, int w, int h);
void hal_camera_brightness(int level);   // -2..2
void hal_camera_capture(void);           // asíncrono -> core_on_photo(ok)
// Dibuja la última foto sacada (escalada) en el framebuffer. 1 si había foto.
int  hal_photo_draw(uint16_t* fb, int stride, int x, int y, int w, int h);
void hal_photo_discard(void);
// Imágenes del chat: la plataforma guarda la última foto con un id para mostrarla después
void hal_image_keep(int id);
int  hal_image_draw(int id, uint16_t* fb, int stride, int x, int y, int w, int h);  // 1 si existe
void hal_image_clear(void);

// --- Agente IA ------------------------------------------------------------
// Envía un mensaje (UTF-8) al agente; si with_photo, adjunta la última foto.
// Asíncrono -> core_on_reply(len, ok) con la respuesta en core_io().
void hal_ai_send(const char* utf8, int len, int with_photo);
void hal_ai_reset(void);                 // olvida la conversación

// --- WiFi -----------------------------------------------------------------
// Asíncrono -> core_on_wifi_scan(len); en core_io(): "ssid\trssi\tsegura\n"...
void hal_wifi_scan(void);
// Asíncrono -> core_on_wifi_result(ok); si ok, core_io() trae el nombre de la red.
void hal_wifi_connect(const char* ssid_utf8, const char* pass_utf8);

// --- Matemáticas (en el ESP32 son las de <math.h>) -------------------------
double m_sin(double);   double m_cos(double);   double m_tan(double);
double m_asin(double);  double m_acos(double);  double m_atan(double);
double m_log(double);   double m_log10(double); double m_exp(double);
double m_pow(double, double); double m_sqrt(double);
double m_floor(double); double m_fabs(double);

#ifdef __cplusplus
}
#endif
