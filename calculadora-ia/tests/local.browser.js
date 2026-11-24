// Prueba del simulador local (tools/servidor-local.mjs): el navegador abre la página servida
// en tu computador y usa el relevo de NVIDIA incluido, conectado a NVIDIA real.
// Uso: node tests/local.browser.js
"use strict";
const { chromium } = require("playwright");
const { spawn } = require("child_process");
const fs = require("fs");
const path = require("path");

const PORT = 8020;
let pass = 0, fail = 0;
const check = (name, cond, extra) => { if (cond) pass++; else { fail++; console.log("✗ " + name + (extra ? "\n   " + extra : "")); } };

(async () => {
  const env = Object.assign({}, process.env, { NO_BROWSER: "1", PORT: String(PORT), NODE_USE_ENV_PROXY: "1" });
  if (fs.existsSync("/root/.ccr/ca-bundle.crt")) env.NODE_EXTRA_CA_CERTS = "/root/.ccr/ca-bundle.crt";
  const server = spawn(process.execPath, [path.join(__dirname, "..", "tools", "servidor-local.mjs")], { env, stdio: "ignore" });
  const base = `http://127.0.0.1:${PORT}/`;
  for (let i = 0; i < 50; i++) {
    try { if ((await fetch(base + "nvidia/health")).ok) break; } catch (e) { /* todavía no */ }
    await new Promise((z) => setTimeout(z, 200));
  }

  const browser = await chromium.launch();
  const page = await browser.newPage();
  page.on("pageerror", (e) => { fail++; console.log("✗ error de JavaScript: " + e.message); });
  const outside = [];
  page.on("request", (r) => { if (!r.url().startsWith(base)) outside.push(r.url()); });
  await page.goto(base);
  await page.waitForTimeout(600);

  check("abre con NVIDIA elegido", (await page.inputValue("#provider")) === "nvidia");
  check("detecta el relevo local", /Relevo local activo/.test(await page.getAttribute("#relay", "placeholder")));
  check("explica que no hace falta Cloudflare", /ya viene incluido/.test(await page.textContent("#relaynote")));

  await page.fill("#apikey", "nvapi-invalida");
  await page.click("#testkey");
  await page.waitForFunction(() => !document.getElementById("testkey").disabled, null, { timeout: 60000 });
  const st = await page.textContent("#keystatus");
  check("key inválida detectada con la respuesta real de NVIDIA", /inválida/.test(st), st);
  const model = await page.inputValue("#model");
  check("lista real de modelos y uno de NVIDIA con fotos por defecto", /^nvidia\/.*(vl|omni|vision)/.test(model), model);

  await page.click("#savekey");
  await page.click("#combo");
  await page.fill("#quick", "hola"); await page.press("#quick", "Enter");
  await page.waitForTimeout(4000);
  const chat = await page.evaluate(() => {
    const n = calc.core.core_debug_ai();
    let s = ""; for (const b of new Uint8Array(calc.core.memory.buffer, calc.core.core_io(), n)) s += String.fromCharCode(b);
    return s;
  });
  check("el chat muestra el error real de NVIDIA", chat.includes("API key de NVIDIA inválida"), chat);
  check("todo pasa por tu computador (nada va a Cloudflare ni a otro servidor)", outside.length === 0, outside.join(", "));

  await browser.close();
  server.kill();
  console.log(`\n${pass} comprobaciones OK, ${fail} fallaron`);
  process.exit(fail ? 1 : 0);
})();
