// servidor-local.mjs - El simulador y el relevo de NVIDIA en tu computador, sin Cloudflare.
//
// Sirve la carpeta simulador/ en http://127.0.0.1:8000 y, en la misma dirección, reenvía
// /nvidia/v1/... a la API de NVIDIA. Como todo sale de la misma dirección, el navegador no
// bloquea nada y el simulador detecta el relevo solo. Tu API key no se guarda aquí.
//
// Uso: doble clic en "Abrir simulador local.bat" (Windows) o: node tools/servidor-local.mjs
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "simulador");
const NVIDIA = "https://integrate.api.nvidia.com";
const FIRST_PORT = Number(process.env.PORT || 8000);
const TYPES = {
  ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".mjs": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8", ".wasm": "application/wasm", ".json": "application/json",
  ".png": "image/png", ".svg": "image/svg+xml", ".ico": "image/x-icon",
};

function send(res, status, type, body, extra) {
  res.writeHead(status, Object.assign({ "content-type": type, "cache-control": "no-store" }, extra || {}));
  res.end(body);
}

async function relay(req, res, apiPath) {
  const chunks = [];
  for await (const c of req) chunks.push(c);
  try {
    const upstream = await fetch(NVIDIA + apiPath, {
      method: req.method,
      headers: {
        "authorization": req.headers["authorization"] || "",
        "content-type": "application/json",
        "accept": "application/json",
      },
      body: req.method === "POST" ? Buffer.concat(chunks) : undefined,
      signal: AbortSignal.timeout(150000),
    });
    const extra = {};
    const retry = upstream.headers.get("retry-after");
    if (retry) extra["retry-after"] = retry;
    send(res, upstream.status, upstream.headers.get("content-type") || "application/json",
      Buffer.from(await upstream.arrayBuffer()), extra);
  } catch (e) {
    send(res, 502, "application/json", JSON.stringify({ error: { message: "No se pudo conectar con NVIDIA: " + e.message } }));
  }
}

function serveFile(req, res, urlPath) {
  let rel;
  try { rel = decodeURIComponent(urlPath).replace(/^\/+/, ""); } catch (e) { return send(res, 400, "text/plain", "Mala dirección"); }
  if (!rel) rel = "index.html";
  const full = path.resolve(ROOT, rel);
  if (full !== ROOT && !full.startsWith(ROOT + path.sep)) return send(res, 403, "text/plain", "No permitido");
  fs.readFile(full, (err, data) => {
    if (err) return send(res, 404, "text/plain; charset=utf-8", "No encontrado");
    send(res, 200, TYPES[path.extname(full).toLowerCase()] || "application/octet-stream", data);
  });
}

const server = http.createServer((req, res) => {
  const urlPath = new URL(req.url, "http://localhost").pathname;
  if (urlPath === "/nvidia/health") return send(res, 200, "application/json", JSON.stringify({ ok: true, relay: "local" }));
  const m = urlPath.match(/^\/nvidia(\/v1\/(?:chat\/completions|models))$/);
  if (m) {
    if (req.method !== "GET" && req.method !== "POST") return send(res, 405, "text/plain", "Método no permitido");
    return relay(req, res, m[1]);
  }
  if (req.method !== "GET" && req.method !== "HEAD") return send(res, 405, "text/plain", "Método no permitido");
  serveFile(req, res, urlPath);
});

function openBrowser(url) {
  if (process.env.NO_BROWSER) return;
  const cmd = process.platform === "win32" ? ["cmd", ["/c", "start", "", url]]
    : process.platform === "darwin" ? ["open", [url]] : ["xdg-open", [url]];
  try { spawn(cmd[0], cmd[1], { detached: true, stdio: "ignore" }).on("error", () => {}).unref(); } catch (e) { /* sin navegador */ }
}

let port = FIRST_PORT;
server.on("error", (e) => {
  if (e.code === "EADDRINUSE" && port < FIRST_PORT + 10) { port++; server.listen(port, "127.0.0.1"); return; }
  console.error("No se pudo iniciar el servidor: " + e.message);
  process.exit(1);
});
server.on("listening", () => {
  const url = `http://127.0.0.1:${port}/`;
  console.log(`Simulador local con relevo de NVIDIA: ${url}`);
  console.log("Deja esta ventana abierta mientras lo usas. Para apagarlo, ciérrala.");
  openBrowser(url);
});
server.listen(port, "127.0.0.1");
