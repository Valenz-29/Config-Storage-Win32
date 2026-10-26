# Plot Equipe 3V

Diseños de vinil para unos Salomon Equipe 10 3V Prolink. El número 1 es la tabla de referencia (Brazuca) calcada en vector. Los otros cuatro son variaciones originales. Ninguno lleva los logos de Adidas, la palabra brazuca ni el emblema del Mundial.

| # | Diseño | Eje | Cuándo gana | Lo que cuesta |
| --- | --- | --- | --- | --- |
| 1 | Réplica | Calco fiel; el par es la tabla | Quieres exactamente el diseño de la referencia | Cada ski muestra la mitad del diseño |
| 2 | Órbita | Curvas, inspirado en la referencia | Quieres la idea Brazuca con trazo más limpio | Franjas finas: solo sale impreso |
| 3 | Corte | Ángulos, fondo grafito | Quieres que se vea rápido y que el uso no se note | Es el que más se aleja de la referencia |
| 4 | Contorno | Línea, mínima tinta | Algo sobrio; se puede hacer con vinil de corte | Pierde la energía multicolor |
| 5 | Mural | El par es un solo lienzo | Que se reconozcan desde el telesilla | Cada ski solo es un fragmento |

## Archivos

- `plot-equipe-3v.html`: el prototipo con selector (teclas 1–5 o ← →). Ahí cambias el texto, el largo y los anchos, y alternas entre la vista **Montado** y **Plano de plot**.
- `svg/equipe3v-<diseño>-165cm.svg` y `.pdf`: una hoja por diseño con los dos skis a escala 1:1 en mm. Ambos formatos son vectoriales.
  - Capa `Arte`: el diseño con 3 mm de sangrado.
  - Capa `CutContour`: línea de corte en magenta (#EC008C, 0.25 mm).
  - Capa `Registro`: marcas de centro de bota y rótulos. No se imprime.
  - El texto ya está convertido a contornos (Saira Condensed, licencia OFL).
- `svg/equipe3v-replica-165cm-300dpi.png`: la réplica en imagen a 300 dpi (3567 × 20363 px), solo el arte, para imprentas que piden imagen.
- `trace/`: los scripts que calcan la foto de referencia (`sh trace/run.sh foto.png carpeta`).

## Cómo se hizo la réplica

1. Se endereza la tabla de la foto (estaba inclinada y en perspectiva).
2. Cada píxel se clasifica en blanco, negro, verde, naranja o azul, y se corrige el tono azulado de la foto.
3. Los logos se cubren con el blanco de su lóbulo.
4. Bajo las fijaciones de la tabla, cada franja que entra por arriba se une con su pareja de abajo mediante una curva suave.
5. Cada color se suaviza por separado sobre fondo negro, así las líneas negras entre colores se conservan, y se traza en curvas Bézier con potrace.
6. La tabla se reparte entre los dos skis: puestos juntos, se ven como la tabla.

## Antes de mandar a imprimir

1. Mide tus skis. Las medidas usadas (165 cm, 105/64/94 mm) son una estimación: no encontré la ficha de fábrica del Equipe 10 3V. Si cambian, ajústalas en el prototipo y descarga el SVG otra vez.
2. La zona de fijación y brazos Prolink (magenta en la vista Plano) es aproximada. Desmonta la fijación y recorta el vinil alrededor de los brazos.
3. Pide vinil cast con laminado. El calandrado se encoge con el frío.
4. Aplica desde el centro hacia la punta y la cola, y recorta el sobrante con cúter siguiendo el borde.
