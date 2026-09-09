const CELL_PX = 8;              /* target glyph size; grid density derives from it */
const FRAME_MS = 33;            /* 30fps cap */
const LINES = ["JAMES", "CLARKE"];
const FILL = 0.9;               /* share of the frame the letters may use */
const STROKE = 0.07;            /* extra letter weight, share of the font size */

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

  // letters drawn at screen scale, averaged into one coverage byte per cell
  function mask(cw, ch) {
    const pw = Math.round(W * cw), ph = Math.round(H * ch);
    const cv = document.createElement("canvas");
    cv.width = pw;
    cv.height = ph;
    const ctx = cv.getContext("2d", { willReadFrequently: true });
    let size = (ph * FILL) / (LINES.length * 1.1);
    ctx.font = `bold ${size}px monospace`;
    const widest = Math.max(...LINES.map((s) => ctx.measureText(s).width));
    if (widest + size * STROKE > pw * FILL) {
      size *= (pw * FILL) / (widest + size * STROKE);
      ctx.font = `bold ${size}px monospace`;
    }
    ctx.fillStyle = ctx.strokeStyle = "#fff";
    ctx.lineWidth = size * STROKE;
    ctx.lineJoin = "round";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    const top = (ph - LINES.length * size * 1.1) / 2;
    LINES.forEach((s, i) => {
      const y = top + (i + 0.5) * size * 1.1;
      ctx.strokeText(s, pw / 2, y);
      ctx.fillText(s, pw / 2, y);
    });
    const px = ctx.getImageData(0, 0, pw, ph).data;
    const m = new Uint8Array(e.memory.buffer, e.wf_mask(), W * H);
    for (let y = 0; y < H; y++) {
      const y0 = Math.floor(y * ch), y1 = Math.min(ph, Math.floor((y + 1) * ch));
      for (let x = 0; x < W; x++) {
        const x0 = Math.floor(x * cw), x1 = Math.min(pw, Math.floor((x + 1) * cw));
        let sum = 0, n = 0;
        for (let yy = y0; yy < y1; yy++)
          for (let xx = x0; xx < x1; xx++) { sum += px[(yy * pw + xx) * 4 + 3]; n++; }
        m[y * W + x] = n ? sum / n : 0;
      }
    }
    e.wf_seed();
  }

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
      if (e.wf_init(W, H, (Math.random() * 2 ** 32) >>> 0, cw / CELL_PX) !== 0)
        throw new Error("wf_init failed");
      mask(cw, CELL_PX);
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
      if (rebuild() && reduced) {
        e.wf_settle();
        draw();
      }
    }, 150);
  });

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
