// sim.js - "Plataforma navegador" del núcleo C++ (core.wasm).
// Implementa las funciones hal_* que en la calculadora real hará la XIAO ESP32S3:
// pantalla, teclado, cámara, WiFi y llamadas a la API de Claude.
"use strict";

// ------------------------------------------------------------------ teclas (mismo orden que core/keys.h)
const K = {};
[
  "YEQU", "WINDOW", "ZOOM", "TRACE", "GRAPH", "2ND", "MODE", "DEL", "LEFT", "RIGHT", "UP", "DOWN",
  "ALPHA", "XTTN", "STAT", "MATH", "APPS", "PRGM", "VARS", "CLEAR", "INV", "SIN", "COS", "TAN", "POW",
  "SQR", "COMMA", "LPAREN", "RPAREN", "DIV", "LOG", "7", "8", "9", "MUL", "LN", "4", "5", "6", "SUB",
  "STO", "1", "2", "3", "ADD", "ON", "0", "DOT", "NEG", "ENTER",
].forEach((n, i) => { K[n] = i; });

// [tecla, texto, etiqueta 2nd, etiqueta ALPHA, clase]
const LAYOUT = [
  ["YEQU", "Y=", "STAT PLOT", "F1", "fn"], ["WINDOW", "WINDOW", "TBLSET", "F2", "fn"],
  ["ZOOM", "ZOOM", "FORMAT", "F3", "fn"], ["TRACE", "TRACE", "CALC", "F4", "fn"], ["GRAPH", "GRAPH", "TABLE", "F5", "fn"],
  ["2ND", "2nd", "", "", "k2nd"], ["MODE", "MODE", "QUIT", "", ""], ["DEL", "DEL", "INS", "", ""], "DPAD",
  ["ALPHA", "ALPHA", "A-LOCK", "", "kalpha"], ["XTTN", "X,T,θ,n", "LINK", "", ""], ["STAT", "STAT", "LIST", "", ""],
  ["MATH", "MATH", "TEST", "A", ""], ["APPS", "APPS", "ANGLE", "B", ""], ["PRGM", "PRGM", "DRAW", "C", ""],
  ["VARS", "VARS", "DISTR", "", ""], ["CLEAR", "CLEAR", "", "", ""],
  ["INV", "x⁻¹", "MATRX", "D", ""], ["SIN", "SIN", "SIN⁻¹", "E", ""], ["COS", "COS", "COS⁻¹", "F", ""],
  ["TAN", "TAN", "TAN⁻¹", "G", ""], ["POW", "^", "π", "H", "op"],
  ["SQR", "x²", "√", "I", ""], ["COMMA", ",", "EE", "J", ""], ["LPAREN", "(", "{", "K", ""],
  ["RPAREN", ")", "}", "L", ""], ["DIV", "÷", "e", "M", "op"],
  ["LOG", "LOG", "10ˣ", "N", ""], ["7", "7", "u", "O", "num"], ["8", "8", "v", "P", "num"],
  ["9", "9", "w", "Q", "num"], ["MUL", "×", "[", "R", "op"],
  ["LN", "LN", "eˣ", "S", ""], ["4", "4", "L4", "T", "num"], ["5", "5", "L5", "U", "num"],
  ["6", "6", "L6", "V", "num"], ["SUB", "−", "]", "W", "op"],
  ["STO", "STO→", "RCL", "X", ""], ["1", "1", "L1", "Y", "num"], ["2", "2", "L2", "Z", "num"],
  ["3", "3", "L3", "θ  !", "num"], ["ADD", "+", "MEM", "\"", "op"],
  ["ON", "ON", "OFF", "", "on"], ["0", "0", "CATALOG", "␣", "num"], ["DOT", ".", "i", ":", "num"],
  ["NEG", "(−)", "ANS", "?", "num"], ["ENTER", "ENTER", "ENTRY", "", "op"],
];

// ------------------------------------------------------------------ estado
let core, mem;
const canvas = document.getElementById("screen");
const ctx = canvas.getContext("2d");
const image = ctx.createImageData(320, 240);
const video = document.getElementById("video");
const t0 = performance.now();

const store = {
  get(k) { try { return localStorage.getItem(k); } catch (e) { return null; } },
  set(k, v) { try { if (v == null) localStorage.removeItem(k); else localStorage.setItem(k, v); } catch (e) { /* sin almacenamiento */ } },
};

// ------------------------------------------------------------------ memoria compartida con el núcleo
const enc = new TextEncoder();
const dec = new TextDecoder();
function cString(ptr) {
  const u8 = new Uint8Array(mem.buffer, ptr);
  let n = 0; while (u8[n]) n++;
  return dec.decode(u8.subarray(0, n));
}
function writeIO(str) {
  const cap = core.core_io_cap() - 1;
  let bytes = enc.encode(str);
  if (bytes.length > cap) {                    // corta sin romper un carácter UTF-8
    let n = cap; while (n > 0 && (bytes[n] & 0xC0) === 0x80) n--;
    bytes = bytes.subarray(0, n);
  }
  const u8 = new Uint8Array(mem.buffer, core.core_io(), cap + 1);
  u8.set(bytes); u8[bytes.length] = 0;
  return bytes.length;
}
function readUTF8(ptr, len) { return dec.decode(new Uint8Array(mem.buffer, ptr, len)); }

// Copia un canvas (w x h) al framebuffer RGB565 del núcleo en (x, y)
function blitToFB(fbPtr, stride, x, y, src, w, h) {
  const data = src.getContext("2d").getImageData(0, 0, w, h).data;
  const fb = new Uint16Array(mem.buffer, fbPtr, stride * 240);
  for (let j = 0; j < h; j++) {
    let o = (y + j) * stride + x, i = j * w * 4;
    for (let k = 0; k < w; k++, i += 4) fb[o++] = ((data[i] & 0xF8) << 8) | ((data[i + 1] & 0xFC) << 3) | (data[i + 2] >> 3);
  }
}

// Dibuja "cover" (recorta para llenar) una fuente de imagen en un canvas
function drawCover(c, source, sw, sh, filter) {
  const g = c.getContext("2d", { willReadFrequently: true });
  const s = Math.max(c.width / sw, c.height / sh);
  const dw = sw * s, dh = sh * s;
  g.filter = filter || "none";
  g.drawImage(source, (c.width - dw) / 2, (c.height - dh) / 2, dw, dh);
  g.filter = "none";
}
const scratch = document.createElement("canvas");
function sized(w, h) { scratch.width = w; scratch.height = h; return scratch; }

// ------------------------------------------------------------------ cámara
let stream = null, camLevel = 0, camFallback = null;
let photoCanvas = null, photoB64 = null;

function fakeCameraImage() {               // si no hay cámara: una "hoja de ejercicio" de prueba
  if (camFallback) return camFallback;
  const c = document.createElement("canvas"); c.width = 640; c.height = 480;
  const g = c.getContext("2d");
  g.fillStyle = "#f4f1e8"; g.fillRect(0, 0, 640, 480);
  g.strokeStyle = "#b9d3ea"; for (let y = 60; y < 480; y += 40) { g.beginPath(); g.moveTo(0, y); g.lineTo(640, y); g.stroke(); }
  g.fillStyle = "#1d2a6b"; g.font = "bold 44px Georgia, serif";
  g.fillText("Resuelve:", 40, 110);
  g.fillText("2x + 3 = 11", 140, 220);
  g.font = "26px Georgia, serif"; g.fillStyle = "#555";
  g.fillText("(imagen de prueba: no hay cámara)", 70, 400);
  camFallback = c;
  return c;
}
function cameraSource() {
  if (stream && video.readyState >= 2 && video.videoWidth) return [video, video.videoWidth, video.videoHeight];
  if (stream === "fake") { const c = fakeCameraImage(); return [c, c.width, c.height]; }
  return null;
}
const brightness = () => `brightness(${1 + camLevel * 0.25})`;

// ------------------------------------------------------------------ WiFi
// El navegador no puede elegir redes WiFi: el simulador usa la conexión real de tu
// celular o computador. La "Red de prueba" sirve para practicar la pantalla de contraseña
// (en la calculadora real esa pantalla conecta la XIAO a tu WiFi).
const REAL_NET = "Este equipo (real)";
const TEST_NET = "Red de prueba";
let wifiTimer = null;
const online = () => navigator.onLine !== false;

// ------------------------------------------------------------------ asistentes: Claude o Gemini
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const rgb565 = (r, g, b) => ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
const PROVIDERS = {
  claude: {
    name: "Claude", color: rgb565(217, 119, 87), keyStore: "apikey", keyHint: "sk-ant-...",
    note: "Consigue tu key en console.anthropic.com (necesita crédito, desde USD 5). Se guarda solo en este navegador y se envía únicamente a api.anthropic.com.",
  },
  nvidia: {
    name: "NVIDIA", color: rgb565(118, 185, 0), keyStore: "apikey_nvidia", keyHint: "nvapi-...",
    note: "Consigue tu key en build.nvidia.com (cuenta de desarrollador de NVIDIA; acceso de prueba con límites). En el navegador necesita tu relevo (abajo). La key se guarda solo en este navegador y viaja solo a tu relevo y a NVIDIA.",
  },
  gemini: {
    name: "Gemini", color: rgb565(138, 180, 248), keyStore: "apikey_gemini", keyHint: "AIza...",
    note: "Consigue tu key gratis en aistudio.google.com. En el plan gratis, Google puede usar lo que envías (y personas pueden revisarlo) para mejorar sus productos: no mandes datos personales. La key se guarda solo en este navegador y se envía únicamente a generativelanguage.googleapis.com.",
  },
};
const CLAUDE_MODELS = [["claude-opus-5-5", "Claude Opus 5.5 (el más capaz)"], ["claude-sonnet-5-5", "Claude Sonnet 5.5"],
  ["claude-haiku-5-5", "Claude Haiku 5.5 (el más barato)"]];
const pickProvider = (v) => (v === "gemini" || v === "nvidia" ? v : "claude");
let provider = pickProvider(store.get("provider"));
const currentModel = () => document.getElementById("model").value;

// Solicitud con reintentos (saturación, límites cortos, fallas de red) y tiempo límite.
// retryable(respuesta, intento) devuelve cuántos ms esperar, o null para no reintentar.
async function request(method, url, headers, body, timeoutMs, retryable) {
  let last = null;
  for (let attempt = 0; attempt < 3; attempt++) {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), timeoutMs);
    try {
      const res = await fetch(url, { method, headers, body: body ? JSON.stringify(body) : undefined, signal: ctrl.signal });
      clearTimeout(timer);
      const data = await res.json().catch(() => ({}));
      last = { status: res.status, data, headers: res.headers };
      const wait = res.ok ? null : retryable(last, attempt);
      if (wait == null || attempt === 2) return last;
      await sleep(wait);
    } catch (e) {
      clearTimeout(timer);
      last = { status: 0, timeout: e.name === "AbortError", data: {} };
      if (attempt === 2 || last.timeout) return last;
      await sleep(1500 * (attempt + 1));
    }
  }
  return last;
}

// ------------------------------------------------------------------ Claude (API de Anthropic)
const ANTHROPIC = "https://api.anthropic.com/v1";
let claudeHistory = [];   // la conversación (se manda completa en cada mensaje)

function claudeHeaders(key, model) {
  const h = {
    "content-type": "application/json",
    "x-api-key": key,
    "anthropic-version": "2026-10-21",
    "anthropic-dangerous-direct-browser-access": "true",
  };
  if (model && model !== "claude-haiku-5-5") h["anthropic-beta"] = "server-side-fallback-2026-07-01";
  return h;
}

function claudeRetry(r, attempt) {
  if (r.status !== 429 && r.status !== 529 && r.status < 500) return null;
  const after = parseFloat(r.headers && r.headers.get("retry-after"));
  return Math.min(isFinite(after) ? after * 1000 : 1500 * (attempt + 1), 10000);
}

function claudeError(r, model) {
  const msg = (r.data && r.data.error && r.data.error.message) || "";
  if (r.timeout) return "Claude tardó demasiado en responder. Intenta de nuevo.";
  if (r.status === 0) return online() ? "No se pudo conectar con la API de Claude." : "Sin internet. Revisa tu conexión.";
  if (r.status === 400 && /credit/i.test(msg)) return "No tienes crédito en la API. Recarga en console.anthropic.com.";
  if (r.status === 401) return "API key inválida. Revísala en el panel del simulador.";
  if (r.status === 403) return "Tu API key no tiene permiso para usar " + model + ".";
  if (r.status === 404) return "El modelo " + model + " no está disponible para tu cuenta.";
  if (r.status === 413) return "La foto es demasiado grande. Intenta de nuevo.";
  if (r.status === 429) return "Demasiadas solicitudes seguidas. Espera un momento.";
  if (r.status === 529 || r.status >= 500) return "Claude está saturado ahora. Intenta en un rato.";
  return "Error " + r.status + (msg ? ": " + msg.slice(0, 160) : "");
}

async function askClaude(text, withPhoto, key, model) {
  const content = [];
  const hasPhoto = withPhoto && photoB64;
  if (hasPhoto) content.push({ type: "image", source: { type: "base64", media_type: "image/jpeg", data: photoB64 } });
  content.push({ type: "text", text });
  claudeHistory.push({ role: "user", content });
  const body = {
    model,
    max_tokens: 16000,
    system: cString(core.core_system_prompt()),
    messages: claudeHistory,
    output_config: { effort: hasPhoto ? "medium" : "low" },   // con foto piensa un poco más
    cache_control: { type: "ephemeral" },                     // abarata los chats largos y con fotos
  };
  if (model !== "claude-haiku-5-5") body.fallbacks = "default";
  const r = await request("POST", ANTHROPIC + "/messages", claudeHeaders(key, model), body, 90000, claudeRetry);
  if (r.status !== 200) { claudeHistory.pop(); return [false, claudeError(r, model)]; }
  const data = r.data;
  if (data.stop_reason === "refusal") { claudeHistory.pop(); return [false, "Claude no puede responder eso."]; }
  claudeHistory.push({ role: "assistant", content: data.content });
  let out = (data.content || []).filter((b) => b.type === "text").map((b) => b.text).join("\n").trim();
  if (data.stop_reason === "max_tokens") out += " (respuesta cortada)";
  return [true, out || "(sin respuesta)"];
}

// Comprueba la key sin gastar crédito (contar tokens es gratis)
async function testClaude(key, model) {
  const r = await request("POST", ANTHROPIC + "/messages/count_tokens",
    { model, messages: [{ role: "user", content: "hola" }] }, claudeHeaders(key, null), 20000, claudeRetry);
  if (r.status === 200) return [true, "La key funciona con " + model + ". ✓"];
  return [false, claudeError(r, model)];
}

// ------------------------------------------------------------------ Gemini (API de Google)
const GOOGLE = "https://generativelanguage.googleapis.com/v1beta";
let geminiHistory = [];
const geminiHeaders = (key) => ({ "content-type": "application/json", "x-goog-api-key": key });

function geminiDetail(r, type) {
  const d = r.data && r.data.error && r.data.error.details;
  return Array.isArray(d) ? d.find((x) => String(x["@type"] || "").endsWith(type)) : null;
}

function geminiRetry(r, attempt) {
  if (r.status === 500 || r.status === 503 || r.status === 504) return 1500 * (attempt + 1);
  if (r.status === 429) {                    // solo si Google dice que basta esperar unos segundos
    const info = geminiDetail(r, "RetryInfo");
    const s = info ? parseFloat(info.retryDelay) : NaN;
    return isFinite(s) && s <= 10 ? s * 1000 + 300 : null;
  }
  return null;
}

function geminiError(r, model) {
  const msg = (r.data && r.data.error && r.data.error.message) || "";
  const info = geminiDetail(r, "ErrorInfo");
  if (r.timeout) return "Gemini tardó demasiado en responder. Intenta de nuevo.";
  if (r.status === 0) return online() ? "No se pudo conectar con la API de Gemini." : "Sin internet. Revisa tu conexión.";
  if ((info && info.reason === "API_KEY_INVALID") || /API key not valid/i.test(msg)) return "API key de Gemini inválida. Revísala en el panel del simulador.";
  if (/location is not supported/i.test(msg)) return "Gemini no está disponible en tu país con esta cuenta.";
  if (r.status === 403) return "Tu key de Gemini no tiene permiso. Revisa que la API esté activada en tu cuenta de Google.";
  if (r.status === 404) return "El modelo " + model + " no está disponible.";
  if (r.status === 429) return "Llegaste al límite de uso de Gemini por ahora (el plan gratis tiene límites). Espera un rato.";
  if (r.status >= 500) return "Gemini está saturado ahora. Intenta en un rato.";
  return "Error " + r.status + (msg ? ": " + msg.slice(0, 160) : "");
}

// Los modelos de Gemini cambian seguido, así que se le pregunta a Google cuáles hay.
// Se prefiere el "flash" estable más nuevo (rápido y con plan gratis).
const GEMINI_SKIP = /embedding|tts|image|imagen|veo|live|audio|robotics|computer-use|aqa/i;
function geminiRank(id) {
  const v = parseFloat((id.match(/gemini-(\d+(?:\.\d+)?)/) || [])[1] || "0");
  return [/preview|exp/i.test(id) ? 1 : 0, /flash/i.test(id) ? 0 : 1, /lite/i.test(id) ? 1 : 0, -v];
}
function compareModels(a, b) {
  const x = geminiRank(a.id), y = geminiRank(b.id);
  for (let i = 0; i < x.length; i++) if (x[i] !== y[i]) return x[i] - y[i];
  return a.id < b.id ? -1 : a.id > b.id ? 1 : 0;
}
async function geminiModels(key) {
  const list = [];
  let token = "";
  for (let page = 0; page < 3; page++) {
    const url = GOOGLE + "/models?pageSize=100" + (token ? "&pageToken=" + encodeURIComponent(token) : "");
    const r = await request("GET", url, geminiHeaders(key), null, 20000, geminiRetry);
    if (r.status !== 200) return { error: geminiError(r, "") };
    for (const m of r.data.models || []) {
      const id = String(m.name || "").replace(/^models\//, "");
      if (!/gemini/i.test(id) || GEMINI_SKIP.test(id)) continue;
      if (!(m.supportedGenerationMethods || []).includes("generateContent")) continue;
      list.push({ id, label: m.displayName || id });
    }
    token = r.data.nextPageToken;
    if (!token) break;
  }
  if (!list.length) return { error: "Tu key no tiene modelos de Gemini disponibles." };
  list.sort(compareModels);
  store.set("gemini_models", JSON.stringify(list));
  return { list };
}

async function askGemini(text, withPhoto, key) {
  let model = currentModel();
  if (!model || model === "auto") {
    const m = await geminiModels(key);
    if (m.error) return [false, m.error];
    fillModels();
    model = m.list[0].id;
  }
  const parts = [];
  if (withPhoto && photoB64) parts.push({ inlineData: { mimeType: "image/jpeg", data: photoB64 } });
  parts.push({ text });
  geminiHistory.push({ role: "user", parts });
  const body = {
    systemInstruction: { parts: [{ text: cString(core.core_system_prompt()) }] },
    contents: geminiHistory,
    generationConfig: { maxOutputTokens: 8192 },
  };
  const send = (m) => request("POST", GOOGLE + "/models/" + encodeURIComponent(m) + ":generateContent",
    geminiHeaders(key), body, 90000, geminiRetry);
  let r = await send(model);
  if (r.status === 404) {                    // el modelo ya no existe: elegir otro y reintentar
    const m = await geminiModels(key);
    if (!m.error && m.list[0].id !== model) { model = m.list[0].id; fillModels(model); r = await send(model); }
  }
  if (r.status !== 200) { geminiHistory.pop(); return [false, geminiError(r, model)]; }
  const data = r.data, cand = (data.candidates || [])[0];
  const out = cand && cand.content && cand.content.parts;
  if (!out || !out.length) {
    geminiHistory.pop();
    const blocked = (data.promptFeedback && data.promptFeedback.blockReason) ||
      (cand && /SAFETY|PROHIBITED|BLOCKLIST|SPII|RECITATION/.test(cand.finishReason || ""));
    return [false, blocked ? "Gemini no puede responder eso." : "Gemini no respondió nada. Intenta de nuevo."];
  }
  geminiHistory.push(Object.assign({}, cand.content, { role: "model" }));   // tal cual, con sus firmas
  let txt = out.filter((p) => typeof p.text === "string" && !p.thought).map((p) => p.text).join("").trim();
  if (cand.finishReason === "MAX_TOKENS") txt += " (respuesta cortada)";
  return [true, txt || "(sin respuesta)"];
}

// Comprueba la key sin gastar cuota (listar modelos es gratis) y elige el modelo
async function testGemini(key) {
  const m = await geminiModels(key);
  if (m.error) return [false, m.error];
  fillModels();
  return [true, "La key funciona. Modelo: " + m.list[0].label + " ✓"];
}

// ------------------------------------------------------------------ NVIDIA (API compatible con OpenAI)
// NVIDIA no permite llamadas desde páginas web (CORS), así que en el navegador se usa tu
// relevo (tools/nvidia-relay). La calculadora real se conecta directo, sin relevo.
let nvidiaHistory = [];
let localRelay = "";       // si el simulador se abrió con "Abrir simulador local.bat", el relevo viene incluido
const nvidiaRelay = () => (store.get("nvidia_relay") || "").trim().replace(/\/+$/, "") || localRelay;

async function detectLocalRelay() {
  if (!/^(localhost|127\.0\.0\.1|\[::1\])$/.test(location.hostname)) return;
  try {
    const r = await fetch("/nvidia/health", { cache: "no-store" });
    if (r.ok && (await r.json()).ok) localRelay = location.origin + "/nvidia";
  } catch (e) { /* servidor sin relevo (por ejemplo python -m http.server) */ }
}
const nvidiaBase = () => nvidiaRelay() + "/v1";
const NO_RELAY = "El navegador no puede hablar directo con NVIDIA. Pega la dirección de tu relevo en el panel (ver instrucciones).";
const nvidiaHeaders = (key) => Object.assign({ "content-type": "application/json", "accept": "application/json" },
  key ? { authorization: "Bearer " + key } : {});

function nvidiaRetry(r, attempt) {
  if (r.status >= 500) return 1500 * (attempt + 1);
  if (r.status === 429) {
    const a = parseFloat(r.headers && r.headers.get("retry-after"));
    if (isFinite(a)) return a <= 10 ? a * 1000 : null;
    return 2000 * (attempt + 1);
  }
  return null;
}

function nvidiaError(r, model, hadPhoto) {
  const d = r.data || {};
  const msg = String((d.error && (d.error.message || d.error)) || d.detail || d.title || "");
  if (r.timeout) return "NVIDIA tardó demasiado en responder. Intenta de nuevo.";
  if (r.status === 0) return online() ? "No se pudo conectar con tu relevo de NVIDIA. Revisa la dirección." : "Sin internet. Revisa tu conexión.";
  if (r.status === 401 || r.status === 403) return "API key de NVIDIA inválida o sin permiso. Revísala en el panel.";
  if (r.status === 402) return "Se acabaron tus créditos de NVIDIA.";
  if (r.status === 404 || r.status === 410) return "El modelo " + model + " ya no está disponible.";
  if ((r.status === 400 || r.status === 422) && hadPhoto) return "Este modelo no acepta fotos. Elige uno marcado con (fotos).";
  if (r.status === 429) return "Llegaste al límite de uso de NVIDIA por ahora. Espera un rato.";
  if (r.status >= 500) return "NVIDIA está saturado ahora. Intenta en un rato.";
  return "Error " + r.status + (msg ? ": " + msg.slice(0, 160) : "");
}

// NVIDIA cambia y retira modelos seguido: se pide la lista y se dejan solo los de chat.
const NVIDIA_SKIP = /embed|retriever|rerank|safety|guard|clip|detector|pii|starcoder|calibration|translate|reward|parse|ocr|asr|tts|riva|cosmos|gliner|muse|video/i;
const nvidiaVision = (m) => m.model_type === "vlm" || /(^|[-_/])vl([-_]|$)|vision|omni|gemma-3/i.test(m.id);
function nvidiaRank(m) {   // primero los que aceptan fotos, de NVIDIA, sin razonamiento largo y estables
  return [m.vision ? 0 : 1, /^nvidia\//.test(m.id) ? 0 : 1, /reasoning|thinking/i.test(m.id) ? 1 : 0, /preview|exp|beta/i.test(m.id) ? 1 : 0];
}
async function nvidiaModels(key) {
  const r = await request("GET", nvidiaBase() + "/models", nvidiaHeaders(key), null, 20000, nvidiaRetry);
  if (r.status !== 200) return { error: nvidiaError(r, "", false) };
  const list = ((r.data && r.data.data) || []).filter((m) => m && m.id && !NVIDIA_SKIP.test(m.id))
    .map((m) => ({ id: m.id, vision: nvidiaVision(m) }))
    .map((m) => Object.assign(m, { label: m.id + (m.vision ? " (fotos)" : "") }));
  if (!list.length) return { error: "NVIDIA no devolvió modelos de chat." };
  list.sort((a, b) => {
    const x = nvidiaRank(a), y = nvidiaRank(b);
    for (let i = 0; i < x.length; i++) if (x[i] !== y[i]) return x[i] - y[i];
    return a.id < b.id ? -1 : a.id > b.id ? 1 : 0;
  });
  store.set("nvidia_models", JSON.stringify(list));
  return { list };
}

async function askNvidia(text, withPhoto, key) {
  if (!nvidiaRelay()) return [false, NO_RELAY];
  let model = currentModel();
  if (!model || model === "auto") {
    const m = await nvidiaModels(key);
    if (m.error) return [false, m.error];
    fillModels();
    model = m.list[0].id;
  }
  const hasPhoto = withPhoto && photoB64;
  const content = hasPhoto
    ? [{ type: "text", text }, { type: "image_url", image_url: { url: "data:image/jpeg;base64," + photoB64 } }]
    : text;
  nvidiaHistory.push({ role: "user", content });
  const send = () => request("POST", nvidiaBase() + "/chat/completions", nvidiaHeaders(key), {
    model,
    messages: [{ role: "system", content: cString(core.core_system_prompt()) }].concat(nvidiaHistory),
    max_tokens: 4096,
    stream: false,
  }, 120000, nvidiaRetry);
  let r = await send();
  if (r.status === 404 || r.status === 410) {      // modelo retirado: elegir otro y reintentar
    const m = await nvidiaModels(key);
    if (!m.error && m.list[0].id !== model) { model = m.list[0].id; fillModels(model); r = await send(); }
  }
  if (r.status !== 200) { nvidiaHistory.pop(); return [false, nvidiaError(r, model, hasPhoto)]; }
  const ch = ((r.data && r.data.choices) || [])[0];
  let txt = ch && ch.message && typeof ch.message.content === "string" ? ch.message.content : "";
  txt = txt.replace(/<think>[\s\S]*?<\/think>/g, "").replace(/^[\s\S]*<\/think>/, "").trim();   // sin el razonamiento
  if (!txt) { nvidiaHistory.pop(); return [false, "NVIDIA no respondió nada. Intenta de nuevo."]; }
  nvidiaHistory.push({ role: "assistant", content: txt });
  if (ch.finish_reason === "length") txt += " (respuesta cortada)";
  return [true, txt];
}

// Comprueba relevo, modelos y key (hace una solicitud mínima de 1 token)
async function testNvidia(key) {
  if (!nvidiaRelay()) return [false, "Primero pega la dirección de tu relevo (ver instrucciones)."];
  const m = await nvidiaModels(key);
  if (m.error) return [false, m.error];
  fillModels();
  const model = currentModel() && currentModel() !== "auto" ? currentModel() : m.list[0].id;
  const r = await request("POST", nvidiaBase() + "/chat/completions", nvidiaHeaders(key),
    { model, messages: [{ role: "user", content: "hola" }], max_tokens: 1 }, 30000, nvidiaRetry);
  if (r.status === 200) return [true, "La key funciona. Modelo: " + model + " ✓"];
  return [false, nvidiaError(r, model, false)];
}

async function askAI(text, withPhoto) {
  const p = PROVIDERS[provider];
  const key = store.get(p.keyStore);
  if (!key) return [false, "Falta la API key de " + p.name + ". Pégala en el panel del simulador."];
  if (!online()) return [false, "Sin internet. Revisa tu conexión."];
  if (provider === "gemini") return askGemini(text, withPhoto, key);
  if (provider === "nvidia") return askNvidia(text, withPhoto, key);
  return askClaude(text, withPhoto, key, currentModel());
}

// Lista de modelos del proveedor elegido
function fillModels(selected) {
  const sel = document.getElementById("model");
  let opts;
  if (provider === "claude") opts = CLAUDE_MODELS;
  else {
    let list = [];
    try { list = JSON.parse(store.get(provider + "_models") || "[]"); } catch (e) { list = []; }
    opts = list.length ? list.map((m, i) => [m.id, m.label + (i === 0 ? " (recomendado)" : "")])
      : [["auto", "Automático (se elige al probar la key)"]];
  }
  sel.innerHTML = "";
  for (const [v, t] of opts) { const o = document.createElement("option"); o.value = v; o.textContent = t; sel.appendChild(o); }
  const want = selected || store.get("model_" + provider) || (provider === "claude" ? store.get("model") : null);
  if (want && [...sel.options].some((o) => o.value === want)) sel.value = want;
}

// ------------------------------------------------------------------ funciones hal_* (las importa core.wasm)
const hal = {
  hal_millis: () => (performance.now() - t0) >>> 0,

  hal_camera_start() {
    camLevel = 0;
    if (stream) return;
    stream = "starting";
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) { stream = "fake"; return; }
    navigator.mediaDevices.getUserMedia({ video: { facingMode: "environment", width: { ideal: 1280 }, height: { ideal: 960 } }, audio: false })
      .then((s) => {
        if (stream !== "starting") { s.getTracks().forEach((t) => t.stop()); return; }
        stream = s; video.srcObject = s; video.play().catch(() => {});
      })
      .catch(() => { if (stream === "starting") stream = "fake"; });
  },
  hal_camera_stop() {
    if (stream && stream.getTracks) stream.getTracks().forEach((t) => t.stop());
    stream = null; video.srcObject = null;
  },
  hal_camera_preview(fb, stride, x, y, w, h) {
    const src = cameraSource(); if (!src) return 0;
    const c = sized(w, h);
    drawCover(c, src[0], src[1], src[2], brightness());
    blitToFB(fb, stride, x, y, c, w, h);
    return 1;
  },
  hal_camera_brightness(level) { camLevel = level; },
  hal_camera_capture() {
    const src = cameraSource();
    if (!src) { setTimeout(() => core.core_on_photo(0), 0); return; }
    const c = document.createElement("canvas"); c.width = 1024; c.height = 768;   // foto en mejor calidad
    drawCover(c, src[0], src[1], src[2], brightness());
    photoCanvas = c;
    photoB64 = c.toDataURL("image/jpeg", 0.85).split(",")[1];
    setTimeout(() => core.core_on_photo(1), 350);  // pausa similar a la de la cámara real
  },
  hal_photo_draw(fb, stride, x, y, w, h) {
    if (!photoCanvas) return 0;
    const c = sized(w, h);
    drawCover(c, photoCanvas, photoCanvas.width, photoCanvas.height);
    blitToFB(fb, stride, x, y, c, w, h);
    return 1;
  },
  hal_photo_discard() { photoCanvas = null; photoB64 = null; },

  hal_ai_send(ptr, len, withPhoto) {
    const text = readUTF8(ptr, len);
    askAI(text, withPhoto).then(([ok, reply]) => {
      photoCanvas = null; photoB64 = null;
      core.core_on_reply(writeIO(reply), ok ? 1 : 0);
    });
  },
  hal_ai_reset() { claudeHistory = []; geminiHistory = []; nvidiaHistory = []; },

  hal_wifi_scan() {
    clearTimeout(wifiTimer);
    wifiTimer = setTimeout(() => {
      const list = [];
      if (online()) list.push(`${REAL_NET}\t-45\t0`);
      list.push(`${TEST_NET}\t-67\t1`);
      core.core_on_wifi_scan(writeIO(list.join("\n")));
    }, 700);
  },
  hal_wifi_connect(ssidPtr, passPtr) {
    const ssid = cString(ssidPtr), pass = cString(passPtr);
    const ok = online() && (ssid === REAL_NET || (ssid === TEST_NET && pass.length >= 8));
    setTimeout(() => {
      if (ok) { store.set("wifi", ssid); writeIO(ssid); }
      core.core_on_wifi_result(ok ? 1 : 0);
    }, 900);
  },

  m_sin: Math.sin, m_cos: Math.cos, m_tan: Math.tan, m_asin: Math.asin, m_acos: Math.acos, m_atan: Math.atan,
  m_log: Math.log, m_log10: Math.log10, m_exp: Math.exp, m_pow: Math.pow, m_sqrt: Math.sqrt,
  m_floor: Math.floor, m_fabs: Math.abs,
};

// ------------------------------------------------------------------ pantalla
function present() {
  const fb = new Uint16Array(mem.buffer, core.core_fb(), 320 * 240);
  const d = image.data;
  for (let i = 0, j = 0; i < fb.length; i++, j += 4) {
    const p = fb[i];
    d[j] = ((p >> 11) & 31) * 255 / 31; d[j + 1] = ((p >> 5) & 63) * 255 / 63; d[j + 2] = (p & 31) * 255 / 31; d[j + 3] = 255;
  }
  ctx.putImageData(image, 0, 0);
}
function loop() {
  core.core_tick();
  if (core.core_frame_changed()) present();
  requestAnimationFrame(loop);
}

// ------------------------------------------------------------------ teclado en pantalla
const buttons = {};
function press(k, down) { core.core_key(k, down ? 1 : 0); }

function bindKey(btn, k) {
  buttons[k] = btn;
  btn.addEventListener("pointerdown", (e) => {
    if (e.button === 2) return;
    e.preventDefault();
    btn.setPointerCapture(e.pointerId);
    if (btn.classList.contains("held")) { btn.classList.remove("held"); btn.classList.remove("down"); press(k, false); return; }
    btn.classList.add("down"); press(k, true);
  });
  const up = () => { if (btn.classList.contains("held")) return; if (btn.classList.contains("down")) { btn.classList.remove("down"); press(k, false); } };
  btn.addEventListener("pointerup", up);
  btn.addEventListener("pointercancel", up);
  btn.addEventListener("contextmenu", (e) => {     // clic derecho: mantener presionada
    e.preventDefault();
    if (btn.classList.toggle("held")) { btn.classList.add("down"); press(k, true); }
    else { btn.classList.remove("down"); press(k, false); }
  });
}

function buildKeypad() {
  const pad = document.getElementById("keypad");
  for (const item of LAYOUT) {
    if (item === "DPAD") {
      const d = document.createElement("div"); d.className = "dpad";
      for (const [name, cls, txt] of [["UP", "up", "▲"], ["LEFT", "left", "◀"], ["RIGHT", "right", "▶"], ["DOWN", "down", "▼"]]) {
        const b = document.createElement("button"); b.type = "button"; b.className = cls; b.textContent = txt;
        b.setAttribute("aria-label", name); d.appendChild(b); bindKey(b, K[name]);
      }
      pad.appendChild(d); continue;
    }
    const [name, txt, l2, la, cls] = item;
    const w = document.createElement("div"); w.className = "key " + (cls || "");
    if (l2) { const s = document.createElement("span"); s.className = "l2"; s.textContent = l2; w.appendChild(s); }
    if (la) { const s = document.createElement("span"); s.className = "la"; s.textContent = la; w.appendChild(s); }
    const b = document.createElement("button"); b.type = "button"; b.textContent = txt;
    w.appendChild(b); pad.appendChild(w); bindKey(b, K[name]);
  }
}

// Teclado del PC
const PC_KEYS = {
  Enter: "ENTER", Backspace: "DEL", Escape: "CLEAR", End: "ON", ArrowUp: "UP", ArrowDown: "DOWN",
  ArrowLeft: "LEFT", ArrowRight: "RIGHT", F1: "YEQU", F2: "WINDOW", F3: "ZOOM", F4: "TRACE", F5: "GRAPH",
  "+": "ADD", "-": "SUB", "*": "MUL", "/": "DIV", "^": "POW", "(": "LPAREN", ")": "RPAREN", ".": "DOT", ",": "COMMA",
};
for (const d of "0123456789") PC_KEYS[d] = d;

function onKey(e, down) {
  if (e.target instanceof HTMLInputElement || e.target instanceof HTMLSelectElement) return;
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  // En modo IA, lo que se escribe va directo al mensaje
  if (down && core.core_wants_text() && e.key.length === 1) {
    e.preventDefault(); core.core_input_char(e.key.codePointAt(0)); return;
  }
  const name = PC_KEYS[e.key];
  if (!name) return;
  e.preventDefault();
  if (down && e.repeat && !["Backspace", "ArrowUp", "ArrowDown"].includes(e.key)) return;
  if (down && e.repeat) press(K[name], false);
  press(K[name], down);
  const b = buttons[K[name]]; if (b && !b.classList.contains("held")) b.classList.toggle("down", down);
}

// ------------------------------------------------------------------ panel
function setupPanel() {
  const key = document.getElementById("apikey"), status = document.getElementById("keystatus");
  const model = document.getElementById("model"), prov = document.getElementById("provider");
  const label = document.getElementById("keylabel");
  const showStatus = () => {
    const p = PROVIDERS[provider], has = !!store.get(p.keyStore);
    label.textContent = "API key de " + p.name;
    status.textContent = has ? "API key de " + p.name + " guardada en este navegador. ✓" : p.note;
    status.classList.toggle("ok", has);
    key.placeholder = has ? "•••••••• (guardada)" : p.keyHint;
  };
  const applyProvider = (newChat) => {
    prov.value = provider;
    fillModels();
    showStatus();
    const p = PROVIDERS[provider];
    core.core_set_ai(writeIO(p.name), p.color);
    document.getElementById("relaybox").hidden = provider !== "nvidia";
    if (newChat) { claudeHistory = []; geminiHistory = []; nvidiaHistory = []; core.core_ai_new_chat(); }
  };
  prov.addEventListener("change", () => {
    provider = pickProvider(prov.value);
    store.set("provider", provider);
    key.value = "";
    applyProvider(true);
  });
  document.getElementById("savekey").addEventListener("click", () => {
    const v = key.value.trim();
    store.set(PROVIDERS[provider].keyStore, v || null); key.value = ""; showStatus();
  });
  model.addEventListener("change", () => store.set("model_" + provider, model.value));
  const relay = document.getElementById("relay");
  relay.value = store.get("nvidia_relay") || "";
  if (localRelay) {
    relay.placeholder = "Relevo local activo ✓ (no necesitas Cloudflare)";
    document.getElementById("relaynote").textContent =
      "Estás usando el simulador local: el relevo de NVIDIA ya viene incluido. Deja este campo vacío.";
  }
  relay.addEventListener("change", () => { store.set("nvidia_relay", relay.value.trim() || null); });
  applyProvider(false);

  const testBtn = document.getElementById("testkey");
  testBtn.addEventListener("click", async () => {
    const p = PROVIDERS[provider];
    const k = key.value.trim() || store.get(p.keyStore);
    if (!k) { status.textContent = "Primero pega tu API key."; status.classList.remove("ok"); return; }
    testBtn.disabled = true; status.textContent = "Probando la key…"; status.classList.remove("ok");
    const [ok, msg] = provider === "gemini" ? await testGemini(k) : provider === "nvidia" ? await testNvidia(k) : await testClaude(k, model.value);
    if (ok && key.value.trim()) { store.set(p.keyStore, k); key.value = ""; key.placeholder = "•••••••• (guardada)"; }
    status.textContent = msg; status.classList.toggle("ok", ok);
    testBtn.disabled = false;
  });

  // Escribir con el teclado del celular (atajo del simulador para el modo IA)
  const quick = document.getElementById("quick");
  quick.addEventListener("input", () => {
    if (core.core_wants_text()) for (const ch of quick.value) core.core_input_char(ch.codePointAt(0));
    quick.value = "";
  });
  quick.addEventListener("keydown", (e) => {
    if (e.key === "Enter") { e.preventDefault(); press(K.ENTER, true); press(K.ENTER, false); }
    else if (e.key === "Backspace" && !quick.value) { e.preventDefault(); press(K.DEL, true); press(K.DEL, false); }
  });

  document.getElementById("combo").addEventListener("click", () => {
    press(K.YEQU, true); press(K.GRAPH, true); press(K.GRAPH, false); press(K.YEQU, false);
  });
}

// ------------------------------------------------------------------ arranque
async function main() {
  const bytes = await (await fetch("core.wasm")).arrayBuffer();
  const { instance } = await WebAssembly.instantiate(bytes, { env: hal });
  core = instance.exports; mem = core.memory;
  core.core_init();
  buildKeypad();
  await detectLocalRelay();
  if (localRelay && !store.get("provider")) provider = "nvidia";
  setupPanel();
  window.addEventListener("keydown", (e) => onKey(e, true));
  window.addEventListener("keyup", (e) => onKey(e, false));
  // Como la calculadora real: al prender se conecta sola a la red guardada.
  const reconnect = () => { if (online()) { writeIO(store.get("wifi") || REAL_NET); core.core_on_wifi_result(1); } };
  reconnect();
  window.addEventListener("online", reconnect);
  window.addEventListener("offline", () => core.core_on_wifi_result(0));
  window.calc = { core, K, press };                // para pruebas
  loop();
}
main().catch((e) => { document.body.insertAdjacentHTML("afterbegin", `<p style="color:#f88">Error al cargar: ${e.message}. Abre esta página con un servidor local (ver README).</p>`); });
