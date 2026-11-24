// Worker de Cloudflare: sirve el simulador y el relevo de NVIDIA en la misma dirección.
// Los archivos de simulador/ los entrega Cloudflare directo; aquí solo llegan las
// rutas que no son archivos, como /nvidia/... (mismo código que la versión de Pages).
import { onRequest } from "../functions/nvidia/[[path]].js";

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.pathname.startsWith("/nvidia/")) {
      const path = url.pathname.slice("/nvidia/".length).split("/");
      return onRequest({ request, params: { path } });
    }
    return env.ASSETS.fetch(request);
  },
};
