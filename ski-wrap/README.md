# Plot Equipe 3V

Cuatro diseños originales de vinil para unos Salomon Equipe 10 3V Prolink. Los inspira la tabla Brazuca de la referencia, pero no llevan sus logos.

| # | Diseño | Eje | Cuándo gana | Lo que cuesta |
| --- | --- | --- | --- | --- |
| 1 | Órbita | Curvas, fiel a la referencia | Quieres que se reconozca la idea Brazuca | Franjas finas: solo sale impreso |
| 2 | Corte | Ángulos, fondo grafito | Quieres que se vea rápido y que el uso no se note | Es el que más se aleja de la referencia |
| 3 | Contorno | Línea, mínima tinta | Algo sobrio; se puede hacer con vinil de corte | Pierde la energía multicolor |
| 4 | Mural | El par es un solo lienzo | Que se reconozcan desde el telesilla | Cada ski solo es un fragmento |

## Archivos

- `plot-equipe-3v.html`: el prototipo con selector (teclas 1–4 o ← →). Ahí cambias el texto, el largo y los anchos, y alternas entre la vista **Montado** y **Plano de plot**.
- `svg/equipe3v-<diseño>-165cm.svg`: una hoja por diseño con los dos skis a escala 1:1 en mm.
  - Capa `Arte`: el diseño con 3 mm de sangrado.
  - Capa `CutContour`: línea de corte en magenta (#EC008C, 0.25 mm).
  - Capa `Registro`: marcas de centro de bota y rótulos. No se imprime.
  - El texto ya está convertido a contornos (Saira Condensed, licencia OFL).

## Antes de mandar a imprimir

1. Mide tus skis. Las medidas usadas (165 cm, 105/64/94 mm) son una estimación: no encontré la ficha de fábrica del Equipe 10 3V. Si cambian, ajústalas en el prototipo y descarga el SVG otra vez.
2. La zona de fijación y brazos Prolink (magenta en la vista Plano) es aproximada. Desmonta la fijación y recorta el vinil alrededor de los brazos.
3. Pide vinil cast con laminado. El calandrado se encoge con el frío.
4. Aplica desde el centro hacia la punta y la cola, y recorta el sobrante con cúter siguiendo el borde.
