# Calculadora IA (TI-83 Plus + XIAO ESP32S3)

Proyecto recreativo: una TI-83 Plus que funciona como calculadora normal y que, con
la combinación secreta **Y= + GRAPH**, cambia al instante a un **modo IA** con chat
con Claude, WiFi y cámara. **ON** vuelve a la calculadora normal.

## Estructura

```
calculadora-ia/
├── core/            Núcleo en C++: es el MISMO código para el simulador y la calculadora real
│   ├── app.cpp      Teclado, combinación secreta y cambio de modo
│   ├── ti.cpp       Modo normal: pantalla de inicio estilo TI-83 (provisoria, luego va el emulador)
│   ├── ai.cpp       Modo IA: chat, cámara (visor → revisión → adjuntar) y WiFi
│   ├── gfx.cpp      Pantalla 320x240 RGB565 (igual que la TFT ILI9341) y texto
│   ├── hal.h        Lo que cada plataforma debe implementar (cámara, WiFi, API, ...)
│   └── font.h       Fuente 6x12 con tildes y símbolos de la TI (generada)
├── simulador/       Plataforma "navegador": implementa hal.h con la webcam, fetch, etc.
├── tools/           gen_font.py (genera core/font.h)
└── build.sh         Compila core/ a simulador/core.wasm
```

La idea: todo lo que se ve en pantalla y la lógica de los modos está en `core/`.
Para la calculadora real solo hay que escribir la otra mitad de `hal.h` para la
XIAO ESP32S3 (pantalla por SPI, teclado por MCP23017, cámara OV2640, WiFi y HTTPS).

## Probar el simulador

```sh
cd calculadora-ia/simulador
python3 -m http.server 8000
```

Abre <http://localhost:8000>, pega tu API key de Anthropic en el panel y prueba:

1. Haz cálculos normales (`2+3*4 ENTER`, `SIN 30 ) ENTER`, ...).
2. Mantén **Y=** y aprieta **GRAPH** (o usa el botón del panel) → modo IA.
3. **F3** WiFi → elige una red → contraseña de 8 o más caracteres.
4. **F2** foto → **ENTER** saca la foto → **F5** usar → escribe y **ENTER**.
5. **ON** → vuelves a la calculadora tal como la dejaste.

Si no hay cámara, el simulador usa una imagen de prueba con un ejercicio.

## Recompilar el núcleo

Necesitas `clang` con soporte WebAssembly (`wasm-ld`):

```sh
./build.sh
```

## Teclas del modo IA

| Tecla | Acción |
|---|---|
| ALPHA | Cambia entre abc / ABC / 123 |
| 2nd + letra | Tilde: á é í ó ú ñ |
| 2nd + (−) / 2nd + 3 / 2nd + . | ¿ / ¡ / , |
| 0 (en letras) | Espacio |
| DEL / CLEAR | Borrar / borrar todo |
| ▲ ▼ | Desplazarse por el chat |
| F1 (Y=) | Quitar la foto adjunta |
| F2 (WINDOW) | Cámara |
| F3 (ZOOM) | WiFi |
| F4 (TRACE) | Nuevo chat |
| F5 (GRAPH) o ENTER | Enviar |
| ON | Volver a la calculadora normal |

## Próximos pasos

1. Firmware de la XIAO: implementar `hal.h` (TFT, MCP23017, cámara, WiFi, HTTPS a la API).
2. Emulador real de la TI-83 Plus con la ROM de tu propia calculadora (reemplaza `ti.cpp`).
3. Montaje dentro de la TI-83 Plus.
