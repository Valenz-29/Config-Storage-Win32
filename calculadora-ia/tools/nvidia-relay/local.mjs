// local.mjs - Corre el mismo relevo de NVIDIA en tu computador (sin Cloudflare).
// Uso: node tools/nvidia-relay/local.mjs   ->   relevo en http://localhost:8787
// Sirve para probar el simulador en tu computador (http://localhost:8000).
import http from "node:http";
import worker from "./worker.mjs";

const PORT = Number(process.env.PORT || 8787);

http.createServer(async (req, res) => {
  try {
    const chunks = [];
    for await (const c of req) chunks.push(c);
    const headers = {};
    for (const h of ["origin", "authorization", "content-type"]) if (req.headers[h]) headers[h] = req.headers[h];
    const hasBody = req.method !== "GET" && req.method !== "HEAD" && req.method !== "OPTIONS";
    const request = new Request(`http://localhost:${PORT}${req.url}`, {
      method: req.method, headers, body: hasBody ? Buffer.concat(chunks) : undefined,
    });
    const response = await worker.fetch(request);
    res.writeHead(response.status, Object.fromEntries(response.headers));
    res.end(Buffer.from(await response.arrayBuffer()));
  } catch (e) {
    res.writeHead(502, { "content-type": "text/plain" });
    res.end("Error del relevo: " + e.message);
  }
}).listen(PORT, () => console.log(`Relevo de NVIDIA en http://localhost:${PORT}`));
