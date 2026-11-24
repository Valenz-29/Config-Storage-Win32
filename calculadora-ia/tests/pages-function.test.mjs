// Prueba la función de Cloudflare Pages (functions/nvidia/[[path]].js) contra NVIDIA real.
// Uso: node tests/pages-function.test.mjs
import { onRequest } from "../functions/nvidia/[[path]].js";

let pass = 0, fail = 0;
const check = (n, c, x) => { if (c) pass++; else { fail++; console.log("✗ " + n + (x ? "\n   " + x : "")); } };
const call = (path, init) => onRequest({ request: new Request("https://calculadora-ia.pages.dev/nvidia/" + path, init), params: { path: path.split("/") } });

let r = await call("health");
check("health", r.status === 200 && (await r.json()).relay === "cloudflare");
r = await call("v1/models");
const d = await r.json();
check("lista real de modelos de NVIDIA", r.status === 200 && d.data.length > 5, r.status);
r = await call("v1/chat/completions", { method: "POST", headers: { authorization: "Bearer nvapi-invalida", "content-type": "application/json" },
  body: JSON.stringify({ model: "nvidia/nemotron-3.5-super-vl-120b-a12b", messages: [{ role: "user", content: "hola" }], max_tokens: 1 }) });
check("key inválida: error real de NVIDIA", r.status === 401 || r.status === 403, r.status + " " + (await r.text()).slice(0, 100));
r = await call("v1/embeddings");
check("bloquea otras rutas", r.status === 404);
r = await call("v1/models", { method: "DELETE" });
check("bloquea otros métodos", r.status === 405);
console.log(`\n${pass} comprobaciones OK, ${fail} fallaron`);
process.exit(fail ? 1 : 0);
