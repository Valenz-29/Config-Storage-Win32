# Relevo para usar NVIDIA en el simulador

NVIDIA no permite que una página web llame directo a su API: el navegador bloquea la
llamada (CORS). Para usar NVIDIA en el simulador necesitas un **relevo**: un mini-programa
gratis que recibe los mensajes del simulador, se los pasa a NVIDIA y te devuelve la respuesta.

- Solo acepta llamadas desde tu página (`https://valenz-29.github.io`) y desde `localhost`.
- Solo deja pasar el chat y la lista de modelos.
- **Tu API key no se guarda en el relevo**: viaja en cada mensaje desde tu navegador.
- En la **calculadora real no hace falta**: la XIAO se conecta directo a NVIDIA.

## Crear el relevo en Cloudflare (gratis, desde el celular o el computador)

1. Entra a **dash.cloudflare.com** y crea una cuenta gratis (solo correo y contraseña).
2. En el menú, abre **Workers & Pages** (en algunas cuentas aparece como **Compute → Workers**).
3. Pulsa **Create** → **Create Worker** (o "Start with Hello World").
4. Ponle un nombre, por ejemplo `nvidia-relevo`, y pulsa **Deploy**.
5. Pulsa **Edit code**, borra todo lo que aparece y pega el contenido completo de
   [`worker.mjs`](worker.mjs). Pulsa **Deploy** otra vez.
6. Copia la dirección del Worker. Se ve así: `https://nvidia-relevo.TU-USUARIO.workers.dev`
7. En el simulador: elige **NVIDIA**, pega esa dirección en **Relevo para el navegador**,
   pega tu API key (`nvapi-...`, de **build.nvidia.com**) y pulsa **Probar**.

El plan gratis de Cloudflare permite muchísimas más solicitudes por día de las que vas a usar.

Si abres el simulador desde otra dirección (por ejemplo tu propio dominio), agrégala a la
lista `ALLOWED_ORIGINS` al principio de `worker.mjs` y vuelve a pulsar **Deploy**.

## Usarlo en tu computador (sin Cloudflare)

Con Node.js 18 o más nuevo:

```sh
node calculadora-ia/tools/nvidia-relay/local.mjs     # relevo en http://localhost:8787
cd calculadora-ia/simulador && python3 -m http.server 8000
```

Abre <http://localhost:8000>, elige NVIDIA y pon `http://localhost:8787` como relevo.
