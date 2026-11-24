# Calculadora IA (TI-83 Plus + XIAO ESP32S3)

Proyecto recreativo: una TI-83 Plus que funciona como calculadora normal y que, con
la combinación secreta **Y= + GRAPH**, cambia al instante a un **modo IA** con chat
con Claude, WiFi y cámara. **ON** vuelve a la calculadora normal.

Simulador en línea: <https://valenz-29.github.io/Config-Storage-Win32/calculadora-ia/simulador/>

## Estructura

```
calculadora-ia/
├── core/              Núcleo en C++: es el MISMO código para el simulador y la calculadora real
│   ├── app.cpp        Teclado, combinación secreta y cambio de modo
│   ├── ai.cpp         Modo IA: chat, cámara (visor → revisión → adjuntar) y WiFi
│   ├── gfx.cpp        Pantalla 320x240 RGB565 (igual que la TFT ILI9341) y texto
│   ├── hal.h          Lo que cada plataforma debe implementar (cámara, WiFi, API, ...)
│   ├── font.h         Fuente 6x12 con tildes y símbolos de la TI (generada)
│   ├── font_small.h   Fuente chica 3x6 de la pantalla de gráficas (generada)
│   └── ti/            Modo normal estilo TI-83 Plus
│       ├── eval.cpp   Evaluador con el orden de operaciones de la TI y todas las funciones
│       ├── fmt.cpp    Formato de números (Float/Fix, Normal/Sci/Eng, ►Frac, ►DMS)
│       ├── mathx.cpp  Redondeo a 14 cifras, distribuciones, rand igual que la TI
│       ├── home.cpp   Pantalla de inicio, historial, RCL
│       ├── editor.cpp Edición con cursor, sobrescribir/INS
│       ├── menus.cpp  Menús, MODE/FORMAT/WINDOW/TBLSET y CATALOG
│       ├── graph.cpp  Y=, GRAPH, TRACE, CALC, ZOOM y TABLE
│       ├── lcd.cpp    LCD de 96x64 ampliada x3
│       └── ti.cpp     Navegación, errores (1:Quit 2:Goto), 2nd/ALPHA, ON/OFF
├── simulador/         Plataforma "navegador": implementa hal.h con la cámara, fetch, etc.
├── tests/             Pruebas automáticas (modo normal y modo IA)
├── tools/             gen_font.py (fuentes), nvidia-relay/ (relevo en Cloudflare) y servidor-local (simulador local)
├── Abrir simulador local.bat   Doble clic en Windows: simulador + relevo de NVIDIA en tu PC
└── build.sh           Compila core/ a simulador/core.wasm
```

La idea: todo lo que se ve en pantalla y la lógica de los modos está en `core/`.
Para la calculadora real solo hay que escribir la otra mitad de `hal.h` para la
XIAO ESP32S3 (pantalla por SPI, teclado por MCP23017, cámara OV2640, WiFi y HTTPS).

## Modo normal

Funciona como una TI-83 Plus en todo lo de uso diario:

- **Cálculos** con el orden de operaciones de la TI (multiplicación implícita, negación,
  potencias de izquierda a derecha), redondeo interno a 14 cifras y resultados con 10 cifras
  como la TI (`1/700` → `.0014285714`, `.1+.2-.3` → `0`, `sin(π)` → `0`).
- **Errores** como la TI: `ERR:SYNTAX`, `DIVIDE BY 0`, `DOMAIN`, `NONREAL ANS`, `OVERFLOW`,
  `ARGUMENT`... con `1:Quit` y `2:Goto` (vuelve al lugar del error).
- **Edición**: cursor por token, sobrescribir por defecto, `2nd DEL` (INS), `2nd ◄ ►`,
  `2nd ENTER` (ENTRY, recorre las últimas 10 entradas), `ENTER` repite, `Ans` automático,
  `STO→`, `2nd STO` (RCL), variables A–Z y θ, `:` para varias instrucciones, A-LOCK.
- **MATH**: ►Frac, ►Dec, ³, ³√(, ˣ√, fMin(, fMax(, nDeriv(, fnInt( · **NUM**: abs(, round(,
  iPart(, fPart(, int(, min(, max(, lcm(, gcd( · **PRB**: rand (misma secuencia que la TI),
  nPr, nCr, !, randInt(, randNorm(, randBin(.
- **TEST/LOGIC**, **ANGLE** (°, ', ʳ, ►DMS, R►Pr(, R►Pθ(, P►Rx(, P►Ry(),
  **DISTR** (normal, t, χ², F, binomial, Poisson y geométrica), **CATALOG** (incluye sinh( etc.).
- **MODE**: Normal/Sci/Eng, Float/Fix 0–9, Radian/Degree, Connected/Dot.
- **Gráficas**: Y1–Y0 (activar con el `=`), WINDOW, ZOOM (ZBox, Zoom In/Out, ZDecimal,
  ZSquare, ZStandard, ZTrig, ZInteger, ZoomFit, ZPrevious, ZoomSto/Rcl, SetFactors),
  FORMAT (coordenadas, cuadrícula, ejes, etiquetas), cursor libre, **TRACE**,
  **CALC** (value, zero, minimum, maximum, intersect, dy/dx, ∫f(x)dx con área sombreada),
  **TABLE** y TBLSET, `Y1(3)` en la pantalla de inicio, VARS (Window, Table, Y-VARS, FnOn/FnOff).
- **MEM**: About, Clear Entries, Reset (All RAM / Defaults). **2nd ON** apaga y **ON** prende.

Lo que **todavía no** está (muestra un aviso): STAT y listas, matrices, programas (PRGM),
APPS, DRAW, números complejos y gráficas paramétricas/polares. Eso llega con el
emulador de la TI-83 Plus usando la ROM de tu propia calculadora.

## Modo IA

- Chat con **Claude**, **Gemini** o **NVIDIA** (se elige en el panel del simulador; cada uno con su key).
  - Claude: Opus 5.5 por defecto, también Sonnet 5.5 o Haiku 5.5. La key se saca en
    console.anthropic.com y necesita crédito.
  - Gemini: la key se saca gratis en aistudio.google.com. Los modelos cambian seguido, así que
    el simulador le pregunta a Google cuáles hay y elige el "flash" estable más nuevo. En el
    plan gratis Google puede usar lo que envías para mejorar sus productos: no mandes datos personales.
  - NVIDIA: la key (`nvapi-...`) se saca en build.nvidia.com. NVIDIA no permite llamadas desde
    páginas web, así que el simulador necesita un **relevo** propio y gratis en Cloudflare
    (instrucciones en [`tools/nvidia-relay`](tools/nvidia-relay/README.md)). Sus modelos cambian
    seguido: el simulador pide la lista, marca con "(fotos)" los que aceptan imágenes y, si un
    modelo fue retirado, elige otro solo. En la calculadora real no hace falta el relevo.
- Foto: visor en vivo → revisión (Repetir/Usar) → la foto queda adjunta y escribes el mensaje.
- Reintenta solo si la IA está saturada o hay un corte de red; mensajes de error claros
  (key inválida, sin crédito, límite del plan gratis, sin internet, tiempo límite).
- Con Claude usa caché de la API para que los chats largos y con fotos cuesten menos.
- **WiFi en el simulador:** el navegador no puede elegir redes, así que usa el internet real
  del equipo («Este equipo (real)»). La «Red de prueba» sirve para practicar la pantalla de
  contraseña (cualquier clave de 8 o más caracteres). En la calculadora real esa pantalla
  conecta la XIAO a tu WiFi.

## Probar el simulador

En línea (link de arriba) o en tu computador:

```sh
cd calculadora-ia/simulador
python3 -m http.server 8000
```

Abre <http://localhost:8000>, elige **Claude**, **Gemini** o **NVIDIA**, pega tu API key en el panel y
pulsa **Probar** (verifica la key sin gastar crédito). La key se guarda solo en ese navegador.

## Simulador local en Windows (NVIDIA sin Cloudflare)

Para usar NVIDIA sin crear el relevo en Cloudflare, abre el simulador desde tu PC:

1. En GitHub, en la página del repositorio, pulsa **Code → Download ZIP** y descomprímelo
   (clic derecho → **Extraer todo**).
2. Entra a la carpeta `calculadora-ia` y haz **doble clic en `Abrir simulador local.bat`**.
   - Si Windows muestra "Windows protegió su PC", pulsa **Más información → Ejecutar de todas formas**
     (pasa con cualquier archivo descargado de internet).
3. Se abre el navegador con el simulador y **NVIDIA ya elegido**: el relevo viene incluido, así
   que solo pegas tu key (`nvapi-...`) y pulsas **Probar**.
4. Deja abierta la ventana negra mientras lo usas; para apagarlo, ciérrala.

No hay que instalar nada: si tienes Node.js 18 o más nuevo lo usa, y si no, usa PowerShell
(viene con Windows). Todo funciona solo en tu computador (`http://localhost:8000` o
`http://127.0.0.1:8000`): tu key va directo de tu PC a NVIDIA y no se guarda en el servidor.
La cámara del PC también funciona. Desde el celular sigue usando el link de GitHub Pages
(con Claude o Gemini, o con NVIDIA y el relevo de Cloudflare).

## Recompilar y probar

Necesitas `clang` con soporte WebAssembly (`wasm-ld`) y Node.js:

```sh
./build.sh                      # compila core/ -> simulador/core.wasm
node tests/ti.test.js           # pruebas del modo normal (teclas -> pantalla)
(cd simulador && python3 -m http.server 8765 &) && node tests/ai.browser.js   # modo IA con Claude, Gemini y NVIDIA (Playwright)
node tests/local.browser.js     # simulador local con el relevo de NVIDIA incluido
python3 tools/gen_font.py core/font.h   # solo si cambias las fuentes
```

## Teclas del modo IA

| Tecla | Acción |
|---|---|
| ALPHA | Cambia entre abc / ABC / 123 |
| 2nd + letra | Tilde: á é í ó ú ñ |
| 2nd + (−) / 2nd + 3 / 2nd + . | ¿ / ¡ / , |
| 0 (en letras) | Espacio |
| DEL / CLEAR | Borrar / borrar todo |
| ▲ ▼ | Elegir un mensaje del chat (CLEAR deja de elegir) |
| Y= + ENTER | Con un mensaje con foto elegido: verla en pantalla completa |
| F1 (Y=) | Quitar la foto adjunta |
| F2 (WINDOW) | Cámara |
| F3 (ZOOM) | WiFi |
| F4 (TRACE) | Nuevo chat (pide confirmación: F5 sí, F1 no) |
| F5 (GRAPH) o ENTER | Enviar |
| ON | Volver a la calculadora normal |

## Próximos pasos

1. Firmware de la XIAO: implementar `hal.h` (TFT, MCP23017, cámara, WiFi, HTTPS a la API).
2. Emulador real de la TI-83 Plus con la ROM de tu propia calculadora (reemplaza `core/ti/`).
3. Montaje dentro de la TI-83 Plus.
