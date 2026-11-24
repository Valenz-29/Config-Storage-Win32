// worker.mjs - Relevo para usar la API de NVIDIA desde el simulador web.
//
// El navegador no puede llamar directo a integrate.api.nvidia.com porque NVIDIA no
// permite llamadas desde páginas web (CORS). Este Worker de Cloudflare (gratis) reenvía
// las llamadas y agrega los permisos. Solo acepta llamadas desde tu página y solo hacia
// el chat y la lista de modelos. Tu API key NO se guarda aquí: viaja en cada llamada
// desde tu navegador.
//
// En la calculadora real no hace falta: la XIAO se conecta directo a NVIDIA.

// Páginas que pueden usar este relevo (agrega la tuya si cambia)
const ALLOWED_ORIGINS = [
  "https://valenz-29.github.io",
  "http://localhost:8000",
  "http://localhost:8765",
];
const UPSTREAM = "https://integrate.api.nvidia.com";
const PATHS = /^\/v1\/(chat\/completions|models)$/;

export default {
  async fetch(request) {
    const origin = request.headers.get("Origin") || "";
    const allowed = ALLOWED_ORIGINS.includes(origin);
    const cors = {
      "Access-Control-Allow-Origin": allowed ? origin : ALLOWED_ORIGINS[0],
      "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
      "Access-Control-Allow-Headers": "authorization, content-type",
      "Access-Control-Expose-Headers": "retry-after",
      "Access-Control-Max-Age": "86400",
      "Vary": "Origin",
    };
    if (!allowed) return new Response("Origen no permitido", { status: 403, headers: cors });
    if (request.method === "OPTIONS") return new Response(null, { status: 204, headers: cors });

    const url = new URL(request.url);
    if (!PATHS.test(url.pathname) || (request.method !== "GET" && request.method !== "POST")) {
      return new Response("Ruta no permitida", { status: 404, headers: cors });
    }
    const upstream = await fetch(UPSTREAM + url.pathname, {
      method: request.method,
      headers: {
        "authorization": request.headers.get("authorization") || "",
        "content-type": "application/json",
        "accept": "application/json",
      },
      body: request.method === "POST" ? await request.text() : undefined,
    });
    const headers = new Headers(cors);
    headers.set("content-type", upstream.headers.get("content-type") || "application/json");
    const retry = upstream.headers.get("retry-after");
    if (retry) headers.set("retry-after", retry);
    return new Response(upstream.body, { status: upstream.status, headers });
  },
};
