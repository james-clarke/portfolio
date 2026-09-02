const CELL_PX = 15;             /* target glyph size; grid density derives from it */
const CUT_R = 2;
const DECAY_REF = 0.95;         /* trail fade tuned at H_REF rows, rescaled to H */
const H_REF = 48;
const FRAME_MS = 33;            /* 30fps cap; rain tops out at 11 rows/s */

async function boot() {
  let instance;
  try {
    ({ instance } = await WebAssembly.instantiateStreaming(fetch("waterfall.wasm")));
  } catch {
    const res = await fetch("waterfall.wasm");
    ({ instance } = await WebAssembly.instantiate(await res.arrayBuffer()));
  }
  const e = instance.exports;

  const frame_el = document.getElementById("frame");
  const base = document.getElementById("field");
  const bright = document.getElementById("bright");
  const dec = new TextDecoder();
  const reduced = matchMedia("(prefers-reduced-motion: reduce)").matches;

  // top three ramp glyphs render on the bright overlay
  const rm = new Uint8Array(e.memory.buffer);
  let rp = e.wf_ramp(), rlen = 0;
  while (rm[rp + rlen]) rlen++;
  const ramp = rm.slice(rp, rp + rlen); /* copy out: memory growth detaches views */
  const is_bright = new Uint8Array(256);
  for (let i = Math.max(0, ramp.length - 3); i < ramp.length; i++) is_bright[ramp[i]] = 1;

  const phone = matchMedia("(max-width: 599px)"); /* matches #frame CSS */
  let W = 0, H = 0, bright_buf = null, row_buf = null, brow_buf = null;

  function rebuild() {
    const fw = frame_el.clientWidth;
    if (!(fw > 0)) return false;
    base.style.fontSize = bright.style.fontSize = `${CELL_PX}px`;
    base.textContent = "@".repeat(100);
    const cw = base.getBoundingClientRect().width / 100;
    if (!(cw > 0)) return false;
    const aspect = phone.matches ? 0.9 : 4 / 7; /* taller frame on phones */
    const w = Math.max(20, Math.floor(fw / cw));
    const h = Math.max(12, Math.round((fw * aspect) / CELL_PX));
    const changed = w !== W || h !== H;
    if (changed) {
      W = w;
      H = h;
      const decay = Math.pow(DECAY_REF, H_REF / H);
      if (e.wf_init(W, H, (Math.random() * 2 ** 32) >>> 0, decay) !== 0)
        throw new Error("wf_init failed");
      bright_buf = new Uint8Array(W * H);
      row_buf = new Uint8Array((W + 1) * H - 1).fill(32);
      brow_buf = new Uint8Array((W + 1) * H - 1).fill(32);
      for (let y = 1; y < H; y++)
        row_buf[y * (W + 1) - 1] = brow_buf[y * (W + 1) - 1] = 10;
    }
    base.textContent = dec.decode(row_buf);
    bright.textContent = dec.decode(brow_buf);
    const s = fw / base.getBoundingClientRect().width;
    if (Number.isFinite(s) && s > 0)
      base.style.fontSize = bright.style.fontSize = `${CELL_PX * s}px`;
    frame_el.style.height = `${base.getBoundingClientRect().height}px`;
    return changed;
  }

  function draw() {
    const ptr = e.wf_chars();
    const chars = new Uint8Array(e.memory.buffer, ptr, W * H);
    for (let i = 0; i < W * H; i++) {
      const c = chars[i];
      bright_buf[i] = is_bright[c] ? c : 32;
    }
    for (let y = 0; y < H; y++) {
      row_buf.set(chars.subarray(y * W, (y + 1) * W), y * (W + 1));
      brow_buf.set(bright_buf.subarray(y * W, (y + 1) * W), y * (W + 1));
    }
    base.textContent = dec.decode(row_buf);
    bright.textContent = dec.decode(brow_buf);
  }

  rebuild();
  if (reduced) {
    e.wf_settle();
    draw();
  }

  let resize_t = 0;
  addEventListener("resize", () => {
    clearTimeout(resize_t);
    resize_t = setTimeout(() => {
      if (rebuild()) e.wf_settle();
      if (reduced) draw();
    }, 150);
  });

  let px = 0, py = 0, has_prev = false;
  frame_el.addEventListener("pointermove", (ev) => {
    const r = base.getBoundingClientRect();
    if (!(r.width > 0 && r.height > 0)) return;
    let x = Math.floor(((ev.clientX - r.left) / r.width) * W);
    let y = Math.floor(((ev.clientY - r.top) / r.height) * H);
    x = x < 0 ? 0 : x >= W ? W - 1 : x;
    y = y < 0 ? 0 : y >= H ? H - 1 : y;
    if (!has_prev) { px = x; py = y; has_prev = true; }
    e.wf_cut(px, py, x, y, CUT_R);
    px = x; py = y;
    if (reduced) draw();
  });
  frame_el.addEventListener("pointerleave", () => { has_prev = false; });

  if (reduced) return;

  let prev = performance.now();
  function frame(t) {
    requestAnimationFrame(frame);
    if (t - prev < FRAME_MS) return;
    let dt = (t - prev) / 1000;
    prev = t;
    if (dt > 0.1) dt = 0.1;

    e.wf_step(dt);
    draw();
  }
  requestAnimationFrame(frame);
}

boot().catch(() => { document.getElementById("frame").hidden = true; });

const root = document.documentElement;
const theme_btn = document.getElementById("theme");
const dark_pref = matchMedia("(prefers-color-scheme: dark)");

function theme_mode() {
  return root.dataset.theme || (dark_pref.matches ? "dark" : "light");
}

function theme_label() {
  theme_btn.textContent = theme_mode() === "dark" ? "[light]" : "[dark]";
}

theme_btn.addEventListener("click", () => {
  root.dataset.theme = theme_mode() === "dark" ? "light" : "dark";
  try { localStorage.setItem("theme", root.dataset.theme); } catch {}
  theme_label();
});
dark_pref.addEventListener("change", theme_label);
theme_label();
