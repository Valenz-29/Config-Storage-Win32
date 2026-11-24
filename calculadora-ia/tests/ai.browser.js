// Prueba del modo IA en un navegador real (Playwright + Chromium con cámara falsa).
// Uso: (cd simulador && python3 -m http.server 8765) y luego: node tests/ai.browser.js
// La API se intercepta para probar reintentos, errores y el formato de las solicitudes;
// el caso de "key inválida" se reenvía a la API real con curl.
"use strict";
const { chromium } = require("playwright");
const { execFileSync } = require("child_process");

const URL = process.env.SIM_URL || "http://localhost:8765/";
let pass = 0, fail = 0;
const check = (name, cond, extra) => { if (cond) pass++; else { fail++; console.log("✗ " + name + (extra ? "\n   " + extra : "")); } };

function realApi(path, headers, body) {   // reenvía a la API real (sin key válida: solo errores reales)
  const args = ["-sS", "-w", "\n%{http_code}", "https://api.anthropic.com" + path, "-d", body];
  for (const [k, v] of Object.entries(headers)) if (!/^(host|origin|referer|content-length)$/i.test(k)) args.push("-H", `${k}: ${v}`);
  const out = execFileSync("curl", args, { encoding: "utf8" });
  const i = out.lastIndexOf("\n");
  return { status: +out.slice(i + 1), body: out.slice(0, i) };
}

(async () => {
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

  // 8. WiFi: red real y red de prueba con contraseña
  await keys("ZOOM"); await page.waitForTimeout(1000);
  if (process.env.SHOT_DIR) await page.screenshot({ path: process.env.SHOT_DIR + "/ai_wifi.png" });
  await keys("DOWN ENTER"); await page.waitForTimeout(150);
  await type("12345678"); await keys("ENTER"); await page.waitForTimeout(1400);
  check("conecta a la red de prueba con contraseña", await page.evaluate(() => localStorage.getItem("wifi")) === "Red de prueba");

  // 9. sin internet: la barra muestra "sin WiFi" y no se intenta enviar
  const before = sent.length;
  await page.context().setOffline(true); await page.waitForTimeout(200);
  await type("hola"); await keys("ENTER"); await page.waitForTimeout(500);
  check("sin internet no se envía", sent.length === before);
  await page.context().setOffline(false); await page.waitForTimeout(300);
  if (process.env.SHOT_DIR) await page.locator("#screen").screenshot({ path: process.env.SHOT_DIR + "/ai_chat.png" });

  // 10. ON vuelve a la calculadora
  await keys("ON"); await page.waitForTimeout(100);
  check("ON vuelve al modo normal", await page.evaluate(() => calc.core.core_mode() === 0));

  await browser.close();
  console.log(`\n${pass} comprobaciones OK, ${fail} fallaron`);
  process.exit(fail ? 1 : 0);
})();
