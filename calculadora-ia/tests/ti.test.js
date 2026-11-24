// Pruebas del modo calculadora normal: presiona teclas en el núcleo real (core.wasm)
// y compara la pantalla con lo que muestra una TI-83 Plus.
// Uso: node tests/ti.test.js
"use strict";
const fs = require("fs");
const path = require("path");

const KEYS = ["YEQU", "WINDOW", "ZOOM", "TRACE", "GRAPH", "2ND", "MODE", "DEL", "LEFT", "RIGHT", "UP", "DOWN",
  "ALPHA", "XTTN", "STAT", "MATH", "APPS", "PRGM", "VARS", "CLEAR", "INV", "SIN", "COS", "TAN", "POW",
  "SQR", "COMMA", "LPAREN", "RPAREN", "DIV", "LOG", "7", "8", "9", "MUL", "LN", "4", "5", "6", "SUB",
  "STO", "1", "2", "3", "ADD", "ON", "0", "DOT", "NEG", "ENTER"];
const ALIAS = { "+": "ADD", "-": "SUB", "*": "MUL", "/": "DIV", "^": "POW", "(": "LPAREN", ")": "RPAREN",
  ",": "COMMA", ".": "DOT", "(-)": "NEG", "2nd": "2ND", "X": "XTTN", "x2": "SQR", "x-1": "INV", "->": "STO",
  "Y=": "YEQU" };

let now = 0;
const hal = {
  hal_millis: () => now,
  hal_camera_start() {}, hal_camera_stop() {}, hal_camera_preview: () => 0, hal_camera_brightness() {},
  hal_camera_capture() {}, hal_photo_draw: () => 0, hal_photo_discard() {}, hal_ai_send() {}, hal_ai_reset() {},
  hal_wifi_scan() {}, hal_wifi_connect() {},
  m_sin: Math.sin, m_cos: Math.cos, m_tan: Math.tan, m_asin: Math.asin, m_acos: Math.acos, m_atan: Math.atan,
  m_log: Math.log, m_log10: Math.log10, m_exp: Math.exp, m_pow: Math.pow, m_sqrt: Math.sqrt,
  m_floor: Math.floor, m_fabs: Math.abs,
};
const bytes = fs.readFileSync(path.join(__dirname, "..", "simulador", "core.wasm"));
const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), { env: hal });
const core = instance.exports;

// juego de caracteres local -> texto legible
const LOCAL = { 0x80: "⁻¹", 0x81: "📷", 0x82: "→", 0x83: "√", 0x84: "π", 0x85: "E", 0x86: "‾", 0x87: "↑", 0x88: "θ",
  0x89: "≠", 0x8A: "≤", 0x8B: "≥", 0x8C: "►", 0x8D: "ˣ", 0x8E: "Δ", 0x8F: "χ", 0x90: "∫", 0x91: "ʳ", 0x92: "↓", 0x93: "▸" };
function decode(u8) { let s = ""; for (const b of u8) s += LOCAL[b] || String.fromCharCode(b); return s; }

function press(seq) {
  for (const raw of seq.split(/\s+/).filter(Boolean)) {
    const name = ALIAS[raw] || raw.toUpperCase();
    const k = KEYS.indexOf(name);
    if (k < 0) throw new Error("tecla desconocida: " + raw);
    core.core_key(k, 1); core.core_key(k, 0);
    now += 20; core.core_tick();
  }
}
function screen() {
  now += 1000; core.core_tick();             // forzar un redibujado completo
  const n = core.core_debug_ti();
  const s = decode(new Uint8Array(core.memory.buffer, core.core_io(), n));
  const i = s.indexOf("\n", 16 * 8 + 7);
  const rows = s.slice(0, 17 * 8).split("\n").slice(0, 8).map((r) => r.replace(/\s+$/, ""));
  return { rows, small: s.slice(17 * 8), text: rows.join("\n") };
}
function pixel(x, y) {                      // pixel de la LCD de 96x64 leído desde el framebuffer
  const fb = new Uint16Array(core.memory.buffer, core.core_fb(), 320 * 240);
  return fb[(24 + y * 3 + 1) * 320 + 16 + x * 3 + 1] === ((28 & 0xF8) << 8 | (34 & 0xFC) << 3 | 30 >> 3) ? 1 : 0;
}

let pass = 0, fail = 0;
function reset() { core.core_init(); now += 1000; core.core_tick(); }
function test(name, fn) {
  reset();
  try { fn(); pass++; }
  catch (e) { fail++; console.log("✗ " + name + "\n   " + e.message.replace(/\n/g, "\n   ")); }
}
function expectRow(row, want) {
  const sc = screen();
  const got = sc.rows[row].trim();
  if (got !== want) throw new Error(`fila ${row}: esperaba «${want}», salió «${got}»\n${sc.text}`);
}
function expectLast(want) {                 // último resultado (fila no vacía más abajo, a la derecha)
  const sc = screen();
  const rows = sc.rows.filter((r) => r.trim() !== "");
  const got = (rows[rows.length - 1] || "").trim();
  if (got !== want) throw new Error(`esperaba «${want}», salió «${got}»\n${sc.text}`);
}
function calc(keys, want) { press("CLEAR CLEAR " + keys + " ENTER"); expectLast(want); }
function approx(keys, want, tol) {
  press("CLEAR CLEAR " + keys + " ENTER");
  const sc = screen(); const rows = sc.rows.filter((r) => r.trim() !== "");
  const got = parseFloat((rows[rows.length - 1] || "").trim().replace(/‾/g, "-").replace("E", "e"));
  if (!(Math.abs(got - want) <= tol)) throw new Error(`esperaba ≈${want}, salió ${got}\n${sc.text}`);
}
function expectSmall(want) {
  const sc = screen();
  if (!sc.small.includes(want)) throw new Error(`esperaba ver «${want}» en: ${sc.small}`);
}
function expectScreen(want) {
  const sc = screen();
  if (!sc.text.includes(want)) throw new Error(`esperaba ver «${want}» en:\n${sc.text}`);
}

// ------------------------------------------------------------------ aritmética y formato
test("orden de operaciones", () => {
  calc("2 + 3 * 4", "14");
  calc("6 / 2 ( 1 + 2 )", "9");
  calc("2 ^ 3 ^ 2", "64");
  calc("(-) 2 x2", "‾4");
  calc("( (-) 2 ) x2", "4");
  calc("2 ^ (-) 2", ".25");
  calc("(-) 5", "‾5");
  calc("2 (-) 3", "‾6");
  calc("3 - (-) 3", "6");
});
test("decimales como la TI", () => {
  calc("1 / 3", ".3333333333");
  calc("2 / 3", ".6666666667");
  calc("1 / 7 0 0", ".0014285714");
  calc("1 / 7 0 0 0", "1.428571429E‾4");
  calc(". 1 + . 2 - . 3", "0");
  calc("1 2 3 4 5 6 7 8 9 * 1 0 0 0", "1.23456789E11");
  calc("9 9 9 9 9 9 9 9 9 9", "9999999999");
  calc("9 9 9 9 9 9 9 9 9 9 + 1", "1E10");
  calc(". 0 0 1", ".001");
  calc(". 0 0 0 1", "1E‾4");
  calc("2nd ^", "3.141592654");
  calc("2nd /", "2.718281828");
  calc("2 2nd , 3", "2000");
  calc("2nd , (-) 2", ".01");
});
test("funciones del teclado", () => {
  calc("SIN 3 0 )", "‾.9880316241");
  calc("SIN 2nd ^ )", "0");
  calc("COS 0", "1");
  calc("2nd SIN 1 )", "1.570796327");
  calc("LOG 1 0 0 0 )", "3");
  calc("LN 2nd / )", "1");
  calc("2nd LOG 2 )", "100");
  calc("2nd LN 1 )", "2.718281828");
  calc("2nd x2 2 )", "1.414213562");
  calc("4 x-1", ".25");
  calc("(-) 8 ^ ( 1 / 3 )", "‾2");
  calc("(-) 8 ^ ( 2 / 3 )", "‾4");
  calc("( (-) 8 ) ^ ( 2 / 3 )", "4");
  calc("( (-) 8 ) ^ ( 1 / 3 )", "‾2");
  press("CLEAR ( (-) 8 ) ^ . 5 ENTER"); expectRow(0, "ERR:NONREAL ANS"); press("1");
});
test("errores", () => {
  const err = (keys, name) => { press("CLEAR CLEAR " + keys + " ENTER"); expectRow(0, "ERR:" + name); press("1"); };
  err("1 / 0", "DIVIDE BY 0");
  err("2nd x2 (-) 1 )", "NONREAL ANS");
  err("0 ^ 0", "DOMAIN");
  err("LOG 0 )", "DOMAIN");
  err("LOG (-) 1 )", "NONREAL ANS");
  err("1 0 ^ 1 0 0", "OVERFLOW");
  err("7 0 MATH LEFT 4", "OVERFLOW");
  err("MATH RIGHT 9 2 . 5 , 1 )", "DOMAIN");
  err("MATH RIGHT 6 1 )", "ARGUMENT");
  press("CLEAR 2 + + 3 ENTER"); expectRow(0, "ERR:SYNTAX"); expectRow(2, "2:Goto");
});
test("Goto vuelve al error con la entrada", () => {
  press("2 + * 3 ENTER 2");
  expectScreen("2+*3");
  press("DEL ENTER");
  expectLast("5");
});
test("Quit deja la entrada escrita", () => {
  press("2 + * 3 ENTER 1");
  expectRow(0, "2+*3");
});

// ------------------------------------------------------------------ edición
test("sobrescribir, insertar y borrar", () => {
  press("1 2 3 LEFT LEFT DEL ENTER"); expectLast("13");
  press("1 2 3 LEFT LEFT 9 ENTER"); expectLast("193");
  press("1 2 3 LEFT LEFT 2nd DEL 9 ENTER"); expectLast("1923");
  press("1 2 3 2nd LEFT 5 ENTER"); expectLast("523");
});
test("Ans automático y ENTER repetido", () => {
  calc("5", "5");
  press("* 2 ENTER"); expectScreen("Ans*2"); expectLast("10");
  press("ENTER"); expectLast("20");
  press("ENTER"); expectLast("40");
});
test("2nd ENTRY recorre entradas anteriores", () => {
  press("1 + 1 ENTER 2 + 2 ENTER 2nd ENTER"); expectScreen("2+2");
  press("2nd ENTER"); expectScreen("1+1");
});
test("variables, STO→, RCL y dos puntos", () => {
  press("5 -> ALPHA MATH ENTER"); expectLast("5");
  calc("ALPHA MATH * 2", "10");
  calc("2 -> ALPHA APPS ALPHA . ALPHA APPS x2", "4");
  press("CLEAR 2nd -> ALPHA MATH ENTER"); expectLast("5");
  calc("3 -> X ALPHA . X ^ 2", "9");
});
test("A-LOCK y 2nd", () => {
  press("2nd ALPHA MATH APPS ALPHA");
  expectScreen("AB");
});
test("apagar y prender conserva la pantalla", () => {
  calc("2 + 2", "4");
  press("2nd ON"); expectScreen("");
  press("ON"); expectLast("4");
});

// ------------------------------------------------------------------ MODE
test("grados y radianes", () => {
  press("MODE DOWN DOWN RIGHT ENTER 2nd MODE");
  calc("SIN 3 0 )", ".5");
  calc("COS 9 0 )", "0");
  calc("TAN 4 5 )", "1");
  press("CLEAR TAN 9 0 ) ENTER"); expectRow(0, "ERR:DOMAIN");
  press("1"); calc("2nd SIN . 5 )", "30");
});
test("Fix 2", () => {
  press("MODE DOWN RIGHT RIGHT RIGHT ENTER 2nd MODE");
  calc("1 / 3", ".33");
  calc("2", "2.00");
  calc("1 / 1 0 0 0", "1.00E‾3");
  calc("1 2 3 4 . 5 6 7", "1234.57");
});
test("Sci y Eng", () => {
  press("MODE RIGHT ENTER 2nd MODE");
  calc("1 2 3", "1.23E2");
  calc("1", "1E0");
  press("MODE RIGHT RIGHT ENTER 2nd MODE");
  calc("1 2 3 4 5", "12.345E3");
  calc(". 0 0 1", "1E‾3");
});

// ------------------------------------------------------------------ menús
test("MATH: ►Frac, ³, ³√, ˣ√", () => {
  calc(". 7 5 MATH 1", "3/4");
  calc("1 / 3 MATH 1", "1/3");
  calc("2nd ^ MATH 1", "3.141592654");
  calc("2 MATH 3", "8");
  calc("MATH 4 2 7 )", "3");
  calc("3 MATH 5 8", "2");
  calc("3 MATH 5 (-) 8", "‾2");
});
test("MATH: nDeriv, fnInt, fMin, fMax", () => {
  calc("MATH 8 X ^ 3 , X , 2 )", "12.000001");
  calc("MATH 9 X x2 , X , 0 , 3 )", "9");
  calc("MATH 9 1 / X , X , 1 , 2 )", ".6931471806");
  calc("MATH 6 ( X - 2 ) x2 , X , 0 , 5 )", "2");
  approx("MATH 7 (-) X x2 + 4 X , X , 0 , 5 )", 2, 1e-6);
  approx("MATH 6 SIN X ) , X , (-) 2nd ^ , 2nd ^ )", -Math.PI / 2, 1e-6);
});
test("NUM", () => {
  calc("MATH RIGHT 1 (-) 3 )", "3");
  calc("MATH RIGHT 2 2nd ^ , 2 )", "3.14");
  calc("MATH RIGHT 3 (-) 2 . 5 )", "‾2");
  calc("MATH RIGHT 4 (-) 2 . 5 )", "‾.5");
  calc("MATH RIGHT 5 (-) 2 . 5 )", "‾3");
  calc("MATH RIGHT 6 3 , 7 )", "3");
  calc("MATH RIGHT 7 3 , 7 )", "7");
  calc("MATH RIGHT 8 4 , 6 )", "12");
  calc("MATH RIGHT 9 1 2 , 1 8 )", "6");
});
test("PRB", () => {
  calc("MATH LEFT 1", ".9435974025");
  calc("5 MATH LEFT 2 2", "20");
  calc("5 MATH LEFT 3 2", "10");
  calc("5 MATH LEFT 4", "120");
  calc(". 5 MATH LEFT 4", ".8862269255");
  calc("6 9 MATH LEFT 4", "1.711224524E98");
});
test("TEST y LOGIC", () => {
  calc("2 2nd MATH 3 1", "1");
  calc("2 2nd MATH 1 3", "0");
  calc("2 2nd MATH 2 3", "1");
  calc("1 2nd MATH RIGHT 1 0", "0");
  calc("1 2nd MATH RIGHT 2 0", "1");
  calc("1 2nd MATH RIGHT 3 1", "0");
  calc("2nd MATH RIGHT 4 0 )", "1");
  calc("1 + 1 2nd MATH 1 2", "1");
});
test("ANGLE", () => {
  calc("SIN 3 0 2nd APPS 1 )", ".5");
  calc("1 . 5 2nd APPS 4", "1°30'0\"");
  calc("2nd APPS 5 3 , 4 )", "5");
  calc("2nd APPS 7 2 , 0 )", "2");
});
test("DISTR", () => {
  calc("2nd VARS 2 (-) 1 , 1 )", ".6826894921");
  calc("2nd VARS 3 . 9 7 5 )", "1.959963985");
  calc("2nd VARS 1 0 )", ".3989422804");
  calc("2nd VARS 0 1 0 , . 5 , 5 )", ".24609375");
  calc("2nd VARS ALPHA MATH 1 0 , . 5 , 5 )", ".623046875");
  calc("2nd VARS ALPHA APPS 3 , 2 )", ".2240418077");
  calc("2nd VARS 5 (-) 2 , 2 , 1 0 )", ".9266119652");
  calc("2nd VARS 7 0 , 3 . 8 4 1 4 5 8 8 2 1 , 1 )", ".95");
});
test("MEM: Clear Entries y Reset", () => {
  press("1 + 1 ENTER 2nd + 3 ENTER"); expectLast("Done");
  press("CLEAR 7 -> ALPHA MATH ENTER 2nd + 7 1 2"); expectScreen("RAM cleared");
  calc("ALPHA MATH", "0");
});
test("CATALOG pega funciones", () => {
  press("2nd 0 ENTER (-) 4 ) ENTER"); expectLast("4");
});

// ------------------------------------------------------------------ gráficas
test("Y= y GRAPH dibujan X²", () => {
  press("Y= X x2 GRAPH");
  if (!pixel(47, 31)) throw new Error("no está el vértice de X² en el centro");
  if (!pixel(47, 0) || !pixel(94, 31)) throw new Error("faltan los ejes");
});
test("TRACE muestra coordenadas", () => {
  press("Y= X x2 - 2 TRACE");
  expectSmall("Y1=X²-2"); expectSmall("X=0"); expectSmall("Y=‾2");
  press("RIGHT RIGHT RIGHT RIGHT RIGHT"); expectSmall("X=1.0638298");
});
test("CALC zero, minimum, intersect, dy/dx, ∫", () => {
  press("Y= X x2 - 2 2nd TRACE 2 0 ENTER 3 ENTER 1 ENTER");
  expectSmall("Zero"); expectSmall("X=1.4142136");
  press("2nd TRACE 3 (-) 2 ENTER 2 ENTER 0 ENTER");
  expectSmall("Minimum"); expectSmall("Y=‾2");
  press("Y= DOWN 2 GRAPH 2nd TRACE 5 ENTER ENTER 1 ENTER");
  expectSmall("Intersection"); expectSmall("X=2");
  press("2nd TRACE 6 UP 3 ENTER"); expectSmall("dy/dx=6");
  press("2nd TRACE 7 0 ENTER 3 ENTER"); expectSmall("∫f(x)dx=3");
});
test("TABLE", () => {
  press("Y= X x2 2nd GRAPH");
  expectSmall("X|0|1|2|3|4|5|6|Y1|0|1|4|9|16|25|36|X=0|");
  press("DOWN DOWN RIGHT"); expectSmall("Y1=4");
});
test("WINDOW y ZOOM", () => {
  press("WINDOW DOWN 5 ENTER"); expectScreen("Xmax=5");
  press("ZOOM 6 WINDOW"); expectScreen("Xmax=10");
  press("ZOOM 4 WINDOW"); expectScreen("Xmin=‾4.7");
  press("2nd MODE VARS 1 2 ENTER"); expectLast("4.7");
});
test("Y1( ) en la pantalla de inicio", () => {
  press("Y= X x2 + 1 2nd MODE VARS RIGHT 1 1 ( 3 ) ENTER"); expectLast("10");
});


// ------------------------------------------------------------------ casos difíciles
test("entrada larga se parte en filas", () => {
  press("1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 1 0");
  expectRow(0, "1+2+3+4+5+6+7+8+"); expectRow(1, "9+10");
  press("ENTER"); expectLast("55");
});
test("menú con más de 7 opciones baja con ↓", () => {
  press("2nd VARS"); expectRow(7, "7↓χ²cdf(");
  press("UP"); expectScreen("E:geometcdf(");
});
test("error en Y= lleva al editor Y=", () => {
  press("Y= 2 + * X GRAPH"); expectRow(0, "ERR:SYNTAX");
  press("2"); expectScreen("\\Y1=2+*X".slice(1));
  press("DEL GRAPH");
  if (!pixel(47, 31 - 6)) throw new Error("no se dibujó Y1=2+X");
});
test("ventana inválida", () => {
  press("WINDOW 5 ENTER 1 GRAPH"); expectRow(0, "ERR:WINDOW RANGE");
});
test("= de Y= activa y desactiva la función", () => {
  press("Y= X LEFT LEFT ENTER GRAPH");
  if (pixel(10, 31 - 30)) throw new Error("Y1 desactivada no debería dibujarse");
});
test("cursor libre muestra X e Y", () => {
  press("GRAPH RIGHT RIGHT RIGHT"); expectSmall("X=.42553191|Y=0|");
});
test("ZBox", () => {
  press("ZOOM 1 ENTER RIGHT RIGHT RIGHT RIGHT RIGHT DOWN DOWN DOWN DOWN DOWN ENTER WINDOW");
  expectScreen("Xmin=0"); expectScreen("Xmax=1.063829");
});
test("TBLSET cambia inicio y paso", () => {
  press("Y= X x2 2nd WINDOW 1 0 ENTER . 5 ENTER 2nd GRAPH");
  expectSmall("X|10|10.5|11|11.5|12|12.5|13|Y1|100|110.3|121|132.3|144|156.3|169|X=10|");
});
test("FORMAT GridOn y AxesOff", () => {
  press("2nd ZOOM DOWN DOWN RIGHT ENTER DOWN RIGHT ENTER GRAPH");
  if (pixel(47, 1) || pixel(20, 31)) throw new Error("los ejes deberían estar apagados");
  if (!pixel(47 + 5, 31 - 3)) throw new Error("falta la cuadrícula");
});

console.log(`\n${pass} pruebas OK, ${fail} fallaron`);
process.exit(fail ? 1 : 0);
