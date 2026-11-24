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

// ------------------------------------------------------------------ WiFi simulado
const NETS = [["Casa_Jose", -48, 1], ["Vecino_5G", -71, 1], ["Cafe_Libre", -80, 0], ["Familia_Perez", -63, 1]];
let wifiTimer = null;

// ------------------------------------------------------------------ Claude
let history = [];   // mensajes de la API (la conversación)

async function askClaude(text, withPhoto) {
  const key = store.get("apikey");
  if (!key) return [false, "Falta la API key. Pégala en el panel del simulador (1. Configura Claude)."];
  const model = document.getElementById("model").value;
  const content = [];
  if (withPhoto && photoB64) content.push({ type: "image", source: { type: "base64", media_type: "image/jpeg", data: photoB64 } });
  content.push({ type: "text", text });
  history.push({ role: "user", content });

  const body = {
    model,
    max_tokens: 4096,
    system: cString(core.core_system_prompt()),
    messages: history,
    output_config: { effort: "low" },
  };
  const headers = {
    "content-type": "application/json",
    "x-api-key": key,
    "anthropic-version": "2026-10-21",
    "anthropic-dangerous-direct-browser-access": "true",
  };
  if (model !== "claude-haiku-5-5") {      // si un filtro de seguridad rechaza, reintenta con otro modelo
    body.fallbacks = "default";
    headers["anthropic-beta"] = "server-side-fallback-2026-07-01";
  }
  try {
    const res = await fetch("https://api.anthropic.com/v1/messages", { method: "POST", headers, body: JSON.stringify(body) });
    const data = await res.json().catch(() => ({}));
    if (!res.ok) {
      history.pop();
      const msg = data && data.error && data.error.message ? data.error.message : res.statusText;
      if (res.status === 401) return [false, "API key inválida. Revísala en el panel del simulador."];
      if (res.status === 429) return [false, "Demasiadas solicitudes o sin crédito. Espera un momento."];
      return [false, `Error ${res.status}: ${msg}`];
    }
    if (data.stop_reason === "refusal") { history.pop(); return [false, "Claude no puede responder eso."]; }
    history.push({ role: "assistant", content: data.content });
    const out = data.content.filter((b) => b.type === "text").map((b) => b.text).join("\n").trim();
    return [true, out || "(sin respuesta)"];
  } catch (e) {
    history.pop();
    return [false, "Sin conexión con la API: " + e.message];
  }
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
      const list = NETS.map(([s, r, sec]) => `${s}\t${r + Math.round(Math.random() * 6 - 3)}\t${sec}`).join("\n");
      core.core_on_wifi_scan(writeIO(list));
    }, 900);
  },
  hal_wifi_connect(ssidPtr, passPtr) {
    const ssid = cString(ssidPtr), pass = cString(passPtr);
    const net = NETS.find((n) => n[0] === ssid);
    const ok = net && (!net[2] || pass.length >= 8);
    setTimeout(() => {
      if (ok) { store.set("wifi", ssid); writeIO(ssid); } else store.set("wifi", null);
      core.core_on_wifi_result(ok ? 1 : 0);
    }, 1200);
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
  const saved = store.get("wifi");                 // como la calculadora real: se reconecta sola
  if (saved) { writeIO(saved); core.core_on_wifi_result(1); }
  window.calc = { core, K, press };                // para pruebas
  loop();
}
main().catch((e) => { document.body.insertAdjacentHTML("afterbegin", `<p style="color:#f88">Error al cargar: ${e.message}. Abre esta página con un servidor local (ver README).</p>`); });
