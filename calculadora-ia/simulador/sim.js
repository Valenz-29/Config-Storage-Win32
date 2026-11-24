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

// ------------------------------------------------------------------ Claude
const API = "https://api.anthropic.com/v1";
let history = [];   // la conversación (se manda completa en cada mensaje)

function apiHeaders(key, model) {
  const h = {
    "content-type": "application/json",
    "x-api-key": key,
    "anthropic-version": "2026-10-21",
    "anthropic-dangerous-direct-browser-access": "true",
  };
  if (model && model !== "claude-haiku-5-5") h["anthropic-beta"] = "server-side-fallback-2026-07-01";
  return h;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// POST con reintentos (saturación, límites, fallas de red) y tiempo límite.
async function apiPost(path, body, key, model, timeoutMs) {
  let last = null;
  for (let attempt = 0; attempt < 3; attempt++) {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), timeoutMs);
    try {
      const res = await fetch(API + path, { method: "POST", headers: apiHeaders(key, model), body: JSON.stringify(body), signal: ctrl.signal });
      clearTimeout(timer);
      const data = await res.json().catch(() => ({}));
      last = { status: res.status, data };
      const retry = res.status === 429 || res.status === 529 || res.status >= 500;
      if (!retry || attempt === 2) return last;
      const after = parseFloat(res.headers.get("retry-after"));
      await sleep(Math.min(isFinite(after) ? after * 1000 : 1500 * (attempt + 1), 10000));
    } catch (e) {
      clearTimeout(timer);
      last = { status: 0, timeout: e.name === "AbortError", data: {} };
      if (attempt === 2 || last.timeout) return last;
      await sleep(1500 * (attempt + 1));
    }
  }
  return last;
}

function friendlyError(r, model) {
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

async function askClaude(text, withPhoto) {
  const key = store.get("apikey");
  if (!key) return [false, "Falta la API key. Pégala en el panel del simulador (1. Configura Claude)."];
  if (!online()) return [false, "Sin internet. Revisa tu conexión."];
  const model = document.getElementById("model").value;
  const content = [];
  const hasPhoto = withPhoto && photoB64;
  if (hasPhoto) content.push({ type: "image", source: { type: "base64", media_type: "image/jpeg", data: photoB64 } });
  content.push({ type: "text", text });
  history.push({ role: "user", content });

  const body = {
    model,
    max_tokens: 16000,
    system: cString(core.core_system_prompt()),
    messages: history,
    output_config: { effort: hasPhoto ? "medium" : "low" },   // con foto piensa un poco más
    cache_control: { type: "ephemeral" },                     // abarata los chats largos y con fotos
  };
  if (model !== "claude-haiku-5-5") body.fallbacks = "default";

  const r = await apiPost("/messages", body, key, model, 90000);
  if (r.status !== 200) { history.pop(); return [false, friendlyError(r, model)]; }
  const data = r.data;
  if (data.stop_reason === "refusal") { history.pop(); return [false, "Claude no puede responder eso."]; }
  history.push({ role: "assistant", content: data.content });
  let out = (data.content || []).filter((b) => b.type === "text").map((b) => b.text).join("\n").trim();
  if (data.stop_reason === "max_tokens") out += " (respuesta cortada)";
  return [true, out || "(sin respuesta)"];
}

// Comprueba la key sin gastar crédito (contar tokens es gratis)
async function testKey(key, model) {
  const r = await apiPost("/messages/count_tokens", { model, messages: [{ role: "user", content: "hola" }] }, key, null, 20000);
  if (r.status === 200) return [true, "La key funciona con " + model + ". ✓"];
  return [false, friendlyError(r, model)];
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
    askClaude(text, withPhoto).then(([ok, reply]) => {
      photoCanvas = null; photoB64 = null;
      core.core_on_reply(writeIO(reply), ok ? 1 : 0);
    });
  },
  hal_ai_reset() { history = []; },

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
  const model = document.getElementById("model");
  const showStatus = () => {
    const has = !!store.get("apikey");
    status.textContent = has ? "API key guardada en este navegador. ✓" : "La key se guarda solo en este navegador y se envía únicamente a api.anthropic.com.";
    status.classList.toggle("ok", has);
    key.placeholder = has ? "•••••••• (guardada)" : "sk-ant-...";
  };
  document.getElementById("savekey").addEventListener("click", () => {
    const v = key.value.trim();
    store.set("apikey", v || null); key.value = ""; showStatus();
  });
  const m = store.get("model"); if (m) model.value = m;
  model.addEventListener("change", () => store.set("model", model.value));
  showStatus();

  const testBtn = document.getElementById("testkey");
  testBtn.addEventListener("click", async () => {
    const k = key.value.trim() || store.get("apikey");
    if (!k) { status.textContent = "Primero pega tu API key."; status.classList.remove("ok"); return; }
    testBtn.disabled = true; status.textContent = "Probando la key…"; status.classList.remove("ok");
    const [ok, msg] = await testKey(k, model.value);
    if (ok && key.value.trim()) { store.set("apikey", k); key.value = ""; key.placeholder = "•••••••• (guardada)"; }
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
