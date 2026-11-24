// Prueba del modo IA en un navegador real (Playwright + Chromium con cámara falsa).
// Uso: (cd simulador && python3 -m http.server 8765) y luego: node tests/ai.browser.js
// La API se intercepta para probar reintentos, errores y el formato de las solicitudes;
// el caso de "key inválida" se reenvía a la API real con curl.
"use strict";
const { chromium } = require("playwright");
const { execFileSync, spawn } = require("child_process");
const fs = require("fs");
const path = require("path");

const URL = process.env.SIM_URL || "http://localhost:8765/";
let pass = 0, fail = 0;
const check = (name, cond, extra) => { if (cond) pass++; else { fail++; console.log("✗ " + name + (extra ? "\n   " + extra : "")); } };

function realApi(path, headers, body) {   // reenvía a la API real (sin key válida: solo errores reales)
  return realCall("https://api.anthropic.com" + path, headers, body);
}
function realCall(url, headers, body) {
  const args = ["-sS", "-w", "\n%{http_code}", url];
  if (body) args.push("-d", body);
  for (const [k, v] of Object.entries(headers)) if (!/^(host|origin|referer|content-length)$/i.test(k)) args.push("-H", `${k}: ${v}`);
  const out = execFileSync("curl", args, { encoding: "utf8" });
  const i = out.lastIndexOf("\n");
  return { status: +out.slice(i + 1), body: out.slice(0, i) };
}

(async () => {
  const env = Object.assign({}, process.env, { NODE_USE_ENV_PROXY: "1", PORT: "8787" });
  if (fs.existsSync("/root/.ccr/ca-bundle.crt")) env.NODE_EXTRA_CA_CERTS = "/root/.ccr/ca-bundle.crt";
  const relay = spawn(process.execPath, [path.join(__dirname, "..", "tools", "nvidia-relay", "local.mjs")], { env, stdio: "ignore" });
  for (let i = 0; i < 50; i++) {
    try { await fetch("http://localhost:8787/v1/models", { headers: { origin: "http://localhost:8765" } }); break; }
    catch (e) { await new Promise((z) => setTimeout(z, 200)); }
  }
  const browser = await chromium.launch({ args: ["--use-fake-device-for-media-stream", "--use-fake-ui-for-media-stream"] });
  const page = await browser.newPage({ viewport: { width: 1000, height: 900 } });
  page.on("pageerror", (e) => { fail++; console.log("✗ error de JavaScript: " + e.message); });

  const sent = [];
  let script = [];          // respuestas programadas para /v1/messages
  await page.route("https://api.anthropic.com/**", async (route) => {
    const req = route.request();
    const cors = { "access-control-allow-origin": "*" };
    if (req.url().endsWith("/count_tokens")) {
      const r = realApi("/v1/messages/count_tokens", req.headers(), req.postData());
      return route.fulfill({ status: r.status, contentType: "application/json", headers: cors, body: r.body });
    }
    sent.push({ headers: req.headers(), body: JSON.parse(req.postData()) });
    const next = script.shift() || { status: 500, body: {} };
    await new Promise((z) => setTimeout(z, 150));
    route.fulfill({ status: next.status, contentType: "application/json", headers: cors, body: JSON.stringify(next.body) });
  });

  const gsent = [];
  let gscript = [];          // respuestas programadas para generateContent
  const model = (id, methods) => ({ name: "models/" + id, displayName: id.replace(/-/g, " "), supportedGenerationMethods: methods || ["generateContent", "countTokens"] });
  let gmodels = [model("embedding-001", ["embedContent"]), model("gemini-3.6-flash-preview"), model("gemini-3.5-flash"),
    model("gemini-3.5-flash-lite"), model("gemini-3.5-pro"), model("gemini-3.5-flash-preview-tts")];
  await page.route("https://generativelanguage.googleapis.com/**", async (route) => {
    const req = route.request(), url = req.url();
    const cors = { "access-control-allow-origin": "*" };
    if (req.method() === "GET" && /\/models(\?|$)/.test(url)) {
      if (req.headers()["x-goog-api-key"] === "AIzaInvalidaDePrueba") {     // key inválida: respuesta REAL de Google
        const r = realCall(url, req.headers(), null);
        return route.fulfill({ status: r.status, contentType: "application/json", headers: cors, body: r.body });
      }
      return route.fulfill({ status: 200, contentType: "application/json", headers: cors, body: JSON.stringify({ models: gmodels }) });
    }
    gsent.push({ url, headers: req.headers(), body: JSON.parse(req.postData()) });
    const next = gscript.shift() || { status: 500, body: {} };
    await new Promise((z) => setTimeout(z, 120));
    route.fulfill({ status: next.status, contentType: "application/json", headers: cors, body: JSON.stringify(next.body) });
  });
  const nsent = [];
  let nscript = [], nmodels = null;
  await page.route("http://localhost:8787/**", async (route) => {
    const req = route.request(), url = req.url();
    const auth = req.headers()["authorization"] || "";
    const cors = { "access-control-allow-origin": "*" };
    if (req.method() === "GET" && url.endsWith("/v1/models") && nmodels)
      return route.fulfill({ status: 200, contentType: "application/json", headers: cors, body: JSON.stringify({ object: "list", data: nmodels }) });
    if (req.method() === "POST" && url.endsWith("/v1/chat/completions") && auth === "Bearer nvapi-prueba") {
      nsent.push({ url, headers: req.headers(), body: JSON.parse(req.postData()) });
      const next = nscript.shift() || { status: 500, body: {} };
      await new Promise((z) => setTimeout(z, 120));
      return route.fulfill({ status: next.status, contentType: "application/json", headers: cors, body: JSON.stringify(next.body) });
    }
    return route.continue();                       // relevo real -> NVIDIA real
  });
  const chatText = () => page.evaluate(() => {
    const n = calc.core.core_debug_ai();
    const u8 = new Uint8Array(calc.core.memory.buffer, calc.core.core_io(), n);
    let s = ""; for (const b of u8) s += String.fromCharCode(b); return s;
  });

  await page.goto(URL);
  await page.waitForTimeout(500);
  const keys = async (seq) => {
    await page.evaluate((seq) => {
      for (const n of seq.split(/\s+/).filter(Boolean)) { calc.press(calc.K[n], true); calc.press(calc.K[n], false); }
    }, seq);
    await page.waitForTimeout(60);
  };
  const type = async (text) => { await page.locator("#quick").fill(text); await page.waitForTimeout(60); };
  const ok = (text) => ({ status: 200, body: { id: "msg_x", type: "message", role: "assistant", stop_reason: "end_turn",
    content: [{ type: "thinking", thinking: "", signature: "x" }, { type: "text", text }] } });

  // 1. combinación secreta y estado de WiFi automático
  await page.click("#combo");
  await page.waitForTimeout(200);
  check("arranca conectado con el internet del equipo", await page.evaluate(() => calc.core.core_mode() === 1));

  // 2. sin key
  await type("hola"); await keys("ENTER"); await page.waitForTimeout(300);
  check("sin key no llama a la API", sent.length === 0);

  // 3. botón Probar con una key inválida -> respuesta REAL de la API (401)
  await page.fill("#apikey", "sk-ant-api03-invalida");
  await page.click("#testkey");
  await page.waitForFunction(() => !document.getElementById("testkey").disabled, null, { timeout: 30000 });
  const status = await page.textContent("#keystatus");
  check("Probar detecta la key inválida (respuesta real)", /inválida/.test(status), status);

  // 4. key guardada, reintento ante saturación (529) y formato de la solicitud
  await page.fill("#apikey", "sk-ant-api03-prueba"); await page.click("#savekey");
  script = [{ status: 529, body: { type: "error", error: { type: "overloaded_error", message: "Overloaded" } } }, ok("¡Hola! ¿En qué te ayudo? 2x≤4 → x≤2")];
  await type("hola claude"); await keys("ENTER");
  await page.waitForTimeout(3500);
  check("reintenta cuando Claude está saturado", sent.length === 2, "solicitudes: " + sent.length);
  const b = sent[sent.length - 1].body, h = sent[sent.length - 1].headers;
  check("modelo por defecto", b.model === "claude-opus-5-5", b.model);
  check("instrucciones del sistema", /calculadora/.test(b.system));
  check("esfuerzo bajo sin foto", b.output_config && b.output_config.effort === "low");
  check("caché activada", b.cache_control && b.cache_control.type === "ephemeral");
  check("fallback ante rechazos", b.fallbacks === "default" && h["anthropic-beta"] === "server-side-fallback-2026-07-01");
  check("encabezados de la API", h["anthropic-version"] === "2026-10-21" && h["anthropic-dangerous-direct-browser-access"] === "true");
  check("mensaje del usuario", b.messages.length === 1 && b.messages[0].content.at(-1).text === "hola claude", JSON.stringify(b.messages));

  // 5. segundo mensaje con foto: lleva la conversación y la imagen
  script = [ok("x = 4")];
  await keys("WINDOW"); await page.waitForTimeout(1500);
  await keys("ENTER"); await page.waitForTimeout(700);
  await keys("GRAPH"); await page.waitForTimeout(200);
  await type("resuelve"); await keys("ENTER"); await page.waitForTimeout(800);
  const b2 = sent[sent.length - 1].body;
  check("la conversación se manda completa", b2.messages.length === 3 && b2.messages[1].role === "assistant");
  check("la respuesta anterior va sin cambios", JSON.stringify(b2.messages[1].content[0]).includes("thinking"));
  const img = b2.messages[2].content[0];
  check("la foto va adjunta como JPEG", img.type === "image" && img.source.media_type === "image/jpeg" && img.source.data.length > 5000,
    JSON.stringify(img).slice(0, 120));
  check("con foto piensa un poco más", b2.output_config.effort === "medium");

  // 6. rechazo: no queda en la conversación
  script = [{ status: 200, body: { stop_reason: "refusal", content: [] } }, ok("ok")];
  await type("algo"); await keys("ENTER"); await page.waitForTimeout(600);
  await type("otra cosa"); await keys("ENTER"); await page.waitForTimeout(600);
  const b3 = sent[sent.length - 1].body;
  check("un rechazo no ensucia la conversación", b3.messages.length === 5 && b3.messages[4].content.at(-1).text === "otra cosa",
    b3.messages.map((m) => m.role).join(","));

  // 7. sin crédito, respuesta cortada
  script = [{ status: 400, body: { type: "error", error: { type: "invalid_request_error", message: "Your credit balance is too low" } } }];
  await type("x"); await keys("ENTER"); await page.waitForTimeout(600);
  script = [{ status: 200, body: { stop_reason: "max_tokens", content: [{ type: "text", text: "Respuesta larga" }] } }];
  await type("y"); await keys("ENTER"); await page.waitForTimeout(600);
  check("cada mensaje hace una sola solicitud (salvo reintentos)", sent.length === 7, "solicitudes: " + sent.length);

  // 7b. elegir mensajes con las flechas, ver la foto en grande (Y= + ENTER) y confirmar "Nuevo"
  {
    let c = await chatText();
    const msgsTxt = c.split("\n").slice(1).filter((l) => /^[<>#] /.test(l));
    const idx = msgsTxt.findIndex((l) => l === "> resuelve");
    check("el mensaje con foto está en el chat", idx >= 0, c);
    for (let i = 0; i < msgsTxt.length - idx; i++) await keys("UP");
    c = await chatText();
    check("las flechas eligen el mensaje con foto", c.includes("sel:" + idx), c.slice(-40));
    if (process.env.SHOT_DIR) await page.locator("#screen").screenshot({ path: process.env.SHOT_DIR + "/sel.png" });
    await keys("YEQU ENTER");
    c = await chatText();
    check("Y= + ENTER abre la foto en pantalla completa", c.includes("#pantalla:7"), c.slice(-40));
    if (process.env.SHOT_DIR) await page.locator("#screen").screenshot({ path: process.env.SHOT_DIR + "/full_img.png" });
    await keys("CLEAR");
    c = await chatText();
    check("cualquier tecla vuelve al chat", c.includes("#pantalla:0"));
    await keys("DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN");
    await keys("TRACE");
    c = await chatText();
    check("Nuevo pide confirmación", c.includes("#pantalla:8"));
    if (process.env.SHOT_DIR) await page.locator("#screen").screenshot({ path: process.env.SHOT_DIR + "/confirm.png" });
    await keys("YEQU");
    c = await chatText();
    check("No conserva el chat", c.includes("#pantalla:0") && c.includes("> resuelve"));
    await keys("TRACE GRAPH");
    c = await chatText();
    check("Sí borra el chat", c.includes("Nuevo chat con") && !c.includes("> resuelve"), c);
  }

  // 8. Gemini: cambiar de asistente, key inválida real, elegir modelo, reintentos y formato
  await page.selectOption("#provider", "gemini");
  await page.waitForTimeout(150);
  let chat = await chatText();
  check("la pantalla dice Gemini y empieza un chat nuevo", chat.startsWith("Gemini\n") && chat.includes("Nuevo chat con Gemini."), chat.slice(0, 200));
  await page.fill("#apikey", "AIzaInvalidaDePrueba");
  await page.click("#testkey");
  await page.waitForFunction(() => !document.getElementById("testkey").disabled, null, { timeout: 30000 });
  let st = await page.textContent("#keystatus");
  check("Probar detecta la key de Gemini inválida (respuesta real de Google)", /inválida/.test(st), st);
  await page.fill("#apikey", "AIzaKeyDePrueba"); await page.click("#testkey");
  await page.waitForFunction(() => !document.getElementById("testkey").disabled, null, { timeout: 10000 });
  st = await page.textContent("#keystatus");
  check("elige el flash estable más nuevo", (await page.inputValue("#model")) === "gemini-3.5-flash" && /funciona/.test(st), st);
  const opts = await page.$$eval("#model option", (os) => os.map((o) => o.value));
  check("no ofrece modelos que no sirven para chatear", !opts.some((o) => /embedding|tts/.test(o)), opts.join(","));

  const gok = (text, extra) => ({ status: 200, body: { candidates: [{ content: { role: "model",
    parts: [{ text: "pensando…", thought: true }, { text, thoughtSignature: "firma" }] }, finishReason: extra || "STOP" }] } });
  gscript = [{ status: 503, body: { error: { code: 503, status: "UNAVAILABLE", message: "The model is overloaded." } } }, gok("¡Hola desde Gemini!")];
  await type("hola gemini"); await keys("ENTER"); await page.waitForTimeout(2600);
  check("reintenta cuando Gemini está saturado", gsent.length === 2, "solicitudes: " + gsent.length);
  let g = gsent[gsent.length - 1];
  check("llama al modelo elegido", g.url.endsWith("/v1beta/models/gemini-3.5-flash:generateContent"), g.url);
  check("manda la key en el encabezado", g.headers["x-goog-api-key"] === "AIzaKeyDePrueba");
  check("instrucciones del sistema", /calculadora/.test(g.body.systemInstruction.parts[0].text));
  check("mensaje del usuario", g.body.contents.length === 1 && g.body.contents[0].role === "user" && g.body.contents[0].parts[0].text === "hola gemini");
  chat = await chatText();
  check("muestra la respuesta sin el pensamiento", chat.includes("< ¡Hola desde Gemini!") && !chat.includes("pensando"), chat);

  gscript = [gok("x = 4")];
  await keys("WINDOW"); await page.waitForTimeout(1500);
  await keys("ENTER"); await page.waitForTimeout(700);
  await keys("GRAPH"); await page.waitForTimeout(200);
  await type("resuelve"); await keys("ENTER"); await page.waitForTimeout(700);
  g = gsent[gsent.length - 1];
  check("la conversación incluye la respuesta anterior tal cual", g.body.contents.length === 3 && g.body.contents[1].role === "model" &&
    g.body.contents[1].parts[1].thoughtSignature === "firma");
  const gi = g.body.contents[2].parts[0];
  check("la foto va como inlineData JPEG", gi.inlineData && gi.inlineData.mimeType === "image/jpeg" && gi.inlineData.data.length > 5000);

  gscript = [{ status: 200, body: { promptFeedback: { blockReason: "SAFETY" } } }, gok("listo")];
  await type("algo"); await keys("ENTER"); await page.waitForTimeout(500);
  await type("otra cosa"); await keys("ENTER"); await page.waitForTimeout(500);
  g = gsent[gsent.length - 1];
  chat = await chatText();
  check("un bloqueo se avisa y no ensucia la conversación", chat.includes("# Gemini no puede responder eso.") &&
    g.body.contents.length === 5 && g.body.contents[4].parts[0].text === "otra cosa", g.body.contents.length + "\n" + chat);

  const before429 = gsent.length;
  gscript = [{ status: 429, body: { error: { code: 429, status: "RESOURCE_EXHAUSTED", message: "Quota exceeded",
    details: [{ "@type": "type.googleapis.com/google.rpc.RetryInfo", retryDelay: "40s" }] } } }];
  await type("x"); await keys("ENTER"); await page.waitForTimeout(500);
  chat = await chatText();
  check("límite del plan gratis: avisa sin insistir", gsent.length === before429 + 1 && chat.includes("límite de uso de Gemini"));

  gmodels = [model("gemini-3.6-flash"), model("gemini-3.6-pro")];
  gscript = [{ status: 404, body: { error: { code: 404, status: "NOT_FOUND", message: "models/gemini-3.5-flash is not found" } } }, gok("con el modelo nuevo")];
  await type("y"); await keys("ENTER"); await page.waitForTimeout(800);
  g = gsent[gsent.length - 1];
  chat = await chatText();
  check("si el modelo ya no existe, elige otro y reintenta", g.url.includes("gemini-3.6-flash:generateContent") &&
    chat.includes("< con el modelo nuevo") && (await page.inputValue("#model")) === "gemini-3.6-flash", g.url);

  // 9. NVIDIA: relevo, lista real de modelos, key inválida real, reintentos, fotos y modelos retirados
  await page.selectOption("#provider", "nvidia");
  await page.waitForTimeout(150);
  chat = await chatText();
  check("la pantalla dice NVIDIA y empieza un chat nuevo", chat.startsWith("NVIDIA\n") && chat.includes("Nuevo chat con NVIDIA."), chat.slice(0, 120));
  check("muestra el campo del relevo", await page.isVisible("#relay"));
  await page.fill("#apikey", "nvapi-invalida");
  await page.click("#testkey");
  await page.waitForFunction(() => !document.getElementById("testkey").disabled, null, { timeout: 10000 });
  st = await page.textContent("#keystatus");
  check("sin relevo pide configurarlo", /relevo/.test(st), st);
  const beforeNoRelay = nsent.length;
  await page.fill("#apikey", "nvapi-invalida"); await page.click("#savekey");
  await type("hola"); await keys("ENTER"); await page.waitForTimeout(300);
  chat = await chatText();
  check("sin relevo avisa y no intenta llamar", nsent.length === beforeNoRelay && chat.includes("no puede hablar directo con NVIDIA"), chat.slice(-200));
  await page.fill("#relay", "http://localhost:8787"); await page.dispatchEvent("#relay", "change");
  await page.fill("#apikey", "nvapi-invalida");
  await page.click("#testkey");
  await page.waitForFunction(() => !document.getElementById("testkey").disabled, null, { timeout: 60000 });
  st = await page.textContent("#keystatus");
  check("key de NVIDIA inválida detectada (respuesta real a través del relevo)", /inválida/.test(st), st);
  const nopts = await page.$$eval("#model option", (os) => os.map((o) => o.value));
  check("lista real de modelos sin embeddings ni filtros", nopts.length > 5 && !nopts.some((o) => /embed|safety|guard/.test(o)), nopts.join(","));
  const nmodel = await page.inputValue("#model");
  check("elige por defecto un modelo de NVIDIA que acepta fotos", /^nvidia\//.test(nmodel) && /vl|omni|vision/.test(nmodel), nmodel);

  await page.fill("#apikey", "nvapi-prueba"); await page.click("#savekey");
  const nok = (text, finish) => ({ status: 200, body: { id: "chatcmpl-x", object: "chat.completion",
    choices: [{ index: 0, message: { role: "assistant", content: text }, finish_reason: finish || "stop" }] } });
  nscript = [{ status: 503, body: { status: 503, title: "Service Unavailable" } }, nok("<think>razonando…</think>¡Hola desde NVIDIA!")];
  await type("hola nvidia"); await keys("ENTER"); await page.waitForTimeout(2600);
  check("reintenta cuando NVIDIA está saturado", nsent.length === 2, "solicitudes: " + nsent.length);
  let n = nsent[nsent.length - 1];
  check("usa el modelo elegido y la key", n.body.model === nmodel && n.headers["authorization"] === "Bearer nvapi-prueba");
  check("instrucciones y mensaje en formato chat", n.body.messages[0].role === "system" && /calculadora/.test(n.body.messages[0].content) &&
    n.body.messages[1].role === "user" && n.body.messages[1].content === "hola nvidia", JSON.stringify(n.body.messages).slice(0, 200));
  chat = await chatText();
  check("muestra la respuesta sin el razonamiento", chat.includes("< ¡Hola desde NVIDIA!") && !chat.includes("razonando"), chat.slice(-200));

  nscript = [nok("x = 4")];
  await keys("WINDOW"); await page.waitForTimeout(1500);
  await keys("ENTER"); await page.waitForTimeout(700);
  await keys("GRAPH"); await page.waitForTimeout(200);
  await type("resuelve"); await keys("ENTER"); await page.waitForTimeout(700);
  n = nsent[nsent.length - 1];
  const last = n.body.messages[n.body.messages.length - 1];
  check("la foto va como image_url en base64", Array.isArray(last.content) && last.content[1].type === "image_url" &&
    last.content[1].image_url.url.startsWith("data:image/jpeg;base64,") && n.body.messages.length === 4,
    JSON.stringify(n.body.messages.map((m) => m.role)));

  nscript = [{ status: 422, body: { status: 422, title: "Unprocessable Entity", detail: "Images are not supported" } }];
  await keys("WINDOW"); await page.waitForTimeout(1500);
  await keys("ENTER"); await page.waitForTimeout(700);
  await keys("GRAPH"); await page.waitForTimeout(200);
  await type("y esta"); await keys("ENTER"); await page.waitForTimeout(700);
  chat = await chatText();
  check("si el modelo no acepta fotos, lo dice", chat.includes("no acepta fotos"), chat.slice(-160));

  nmodels = [{ id: "nvidia/nemotron-4-vl-200b", object: "model", owned_by: "nvidia", model_type: "vlm" }, { id: "nvidia/nemotron-4-embed", object: "model" }];
  nscript = [{ status: 410, body: { status: 410, title: "Gone", detail: "The model has reached its end of life" } }, nok("con el modelo nuevo")];
  await type("z"); await keys("ENTER"); await page.waitForTimeout(900);
  n = nsent[nsent.length - 1];
  chat = await chatText();
  check("si NVIDIA retiró el modelo, elige otro y reintenta", n.body.model === "nvidia/nemotron-4-vl-200b" &&
    chat.includes("< con el modelo nuevo") && (await page.inputValue("#model")) === "nvidia/nemotron-4-vl-200b", n.body.model);

  // 10. WiFi: red real y red de prueba con contraseña
  await keys("ZOOM"); await page.waitForTimeout(1000);
  if (process.env.SHOT_DIR) await page.screenshot({ path: process.env.SHOT_DIR + "/ai_wifi.png" });
  await keys("DOWN ENTER"); await page.waitForTimeout(150);
  await type("12345678"); await keys("ENTER"); await page.waitForTimeout(1400);
  check("conecta a la red de prueba con contraseña", await page.evaluate(() => localStorage.getItem("wifi")) === "Red de prueba");

  // 11. sin internet: la barra muestra "sin WiFi" y no se intenta enviar
  const before = sent.length;
  await page.context().setOffline(true); await page.waitForTimeout(200);
  await type("hola"); await keys("ENTER"); await page.waitForTimeout(500);
  check("sin internet no se envía", sent.length === before);
  await page.context().setOffline(false); await page.waitForTimeout(300);
  if (process.env.SHOT_DIR) await page.locator("#screen").screenshot({ path: process.env.SHOT_DIR + "/ai_chat.png" });

  // 12. ON vuelve a la calculadora
  await keys("ON"); await page.waitForTimeout(100);
  check("ON vuelve al modo normal", await page.evaluate(() => calc.core.core_mode() === 0));

  await browser.close();
  relay.kill();
  console.log(`\n${pass} comprobaciones OK, ${fail} fallaron`);
  process.exit(fail ? 1 : 0);
})();
