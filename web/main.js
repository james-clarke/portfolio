const W = 140;
const H = 48;
const DECAY = 0.95;
const CUT_R = 2;
const BASE_PX = 10;

async function boot() {
  const res = await fetch("waterfall.wasm");
  const { instance } = await WebAssembly.instantiate(await res.arrayBuffer());
  const e = instance.exports;

  if (e.wf_init(W, H, (Math.random() * 2 ** 32) >>> 0, DECAY) !== 0)
    throw new Error("wf_init failed");

  const frame_el = document.getElementById("frame");
  const base = document.getElementById("field");
  const bright = document.getElementById("bright");
  const dec = new TextDecoder();
  const bright_buf = new Uint8Array(W * H);

  // real glyphs before first frame so fit() measures true grid size
  const blank = (" ".repeat(W) + "\n").repeat(H - 1) + " ".repeat(W);
  base.textContent = blank;
  bright.textContent = blank;

  // scale via font-size
  function fit() {
    base.style.fontSize = bright.style.fontSize = `${BASE_PX}px`;
    const s = frame_el.clientWidth / base.getBoundingClientRect().width;
    base.style.fontSize = bright.style.fontSize = `${BASE_PX * s}px`;
    frame_el.style.height = `${base.getBoundingClientRect().height}px`;
  }
  fit();
  addEventListener("resize", fit);

  function draw() {
    const ptr = e.wf_chars();
    const chars = new Uint8Array(e.memory.buffer, ptr, W * H);
    for (let i = 0; i < W * H; i++) {
      const c = chars[i];
      bright_buf[i] = c === 35 || c === 37 || c === 64 ? c : 32; /* # % @ */
    }
    const rows = new Array(H);
    const brows = new Array(H);
    for (let y = 0; y < H; y++) {
      rows[y] = dec.decode(chars.subarray(y * W, (y + 1) * W));
      brows[y] = dec.decode(bright_buf.subarray(y * W, (y + 1) * W));
    }
    base.textContent = rows.join("\n");
    bright.textContent = brows.join("\n");
  }

  if (matchMedia("(prefers-reduced-motion: reduce)").matches) {
    for (let i = 0; i < 40; i++)
      e.wf_step(0.1);
    draw();
    return;
  }

  let px = -1, py = -1;
  frame_el.addEventListener("pointermove", (ev) => {
    const r = base.getBoundingClientRect();
    const x = Math.floor(((ev.clientX - r.left) / r.width) * W);
    const y = Math.floor(((ev.clientY - r.top) / r.height) * H);
    if (px < 0) { px = x; py = y; }
    e.wf_cut(px, py, x, y, CUT_R);
    px = x; py = y;
  });
  frame_el.addEventListener("pointerleave", () => { px = py = -1; });

  let prev = performance.now();
  function frame(t) {
    let dt = (t - prev) / 1000;
    prev = t;
    if (dt > 0.1) dt = 0.1;

    e.wf_step(dt);
    draw();

    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}

boot();

const root = document.documentElement;
const theme_btn = document.getElementById("theme");
const dark_pref = matchMedia("(prefers-color-scheme: dark)");

function theme_mode() {
  return root.dataset.theme || (dark_pref.matches ? "dark" : "light");
}

function theme_label() {
  theme_btn.textContent = theme_mode() === "dark" ? "light" : "dark";
}

theme_btn.addEventListener("click", () => {
  root.dataset.theme = theme_mode() === "dark" ? "light" : "dark";
  try { localStorage.setItem("theme", root.dataset.theme); } catch {}
  theme_label();
});
dark_pref.addEventListener("change", theme_label);
theme_label();
