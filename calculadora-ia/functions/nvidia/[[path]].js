// Relevo de NVIDIA incluido en Cloudflare Pages (Pages Functions).
// Atiende /nvidia/* en la MISMA dirección que el simulador, así el navegador no bloquea
// nada y NVIDIA funciona también desde el celular. Tu API key no se guarda aquí:
// viaja en cada mensaje desde tu navegador.
const NVIDIA = "https://integrate.api.nvidia.com";
const PATHS = /^\/v1\/(chat\/completions|models)$/;

export async function onRequest({ request, params }) {
  const parts = params.path || [];
  const path = "/" + (Array.isArray(parts) ? parts.join("/") : parts);
  const noStore = { "cache-control": "no-store" };
  if (path === "/health") {
    return new Response(JSON.stringify({ ok: true, relay: "cloudflare" }), { headers: { "content-type": "application/json", ...noStore } });
  }
  if (!PATHS.test(path)) return new Response("Ruta no permitida", { status: 404, headers: noStore });
  if (request.method !== "GET" && request.method !== "POST") return new Response("Método no permitido", { status: 405, headers: noStore });
  try {
    const upstream = await fetch(NVIDIA + path, {
      method: request.method,
      headers: {
        "authorization": request.headers.get("authorization") || "",
        "content-type": "application/json",
        "accept": "application/json",
      },
      body: request.method === "POST" ? await request.text() : undefined,
    });
    const headers = new Headers(noStore);
    headers.set("content-type", upstream.headers.get("content-type") || "application/json");
    const retry = upstream.headers.get("retry-after");
    if (retry) headers.set("retry-after", retry);
    return new Response(upstream.body, { status: upstream.status, headers });
  } catch (e) {
    return new Response(JSON.stringify({ error: { message: "No se pudo conectar con NVIDIA: " + e.message } }),
      { status: 502, headers: { "content-type": "application/json", ...noStore } });
  }
}
