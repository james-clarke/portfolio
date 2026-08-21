const W = 140;
const H = 48;
const DECAY = 0.95;
const CUT_R = 2;

async function boot() {
  const res = await fetch("waterfall.wasm");
  const { instance } = await WebAssembly.instantiate(await res.arrayBuffer());
  const e = instance.exports;

  if (e.wf_init(W, H, (Math.random() * 2 ** 32) >>> 0, DECAY) !== 0)
    throw new Error("wf_init failed");

  const frame_el = document.getElementById("frame");
  const pre = document.getElementById("field");
  const dec = new TextDecoder();

  // real glyphs before first frame so fit() measures true grid size
  pre.textContent = (" ".repeat(W) + "\n").repeat(H - 1) + " ".repeat(W);

  // contain: grid scales whole to the frame width, frame height follows
  function fit() {
    const s = frame_el.clientWidth / pre.offsetWidth;
    pre.style.transform = `scale(${s})`;
    frame_el.style.height = `${pre.offsetHeight * s}px`;
  }
  fit();
  addEventListener("resize", fit);

  let px = -1, py = -1;
  frame_el.addEventListener("pointermove", (ev) => {
    const r = pre.getBoundingClientRect();
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

    const ptr = e.wf_chars();
    const chars = new Uint8Array(e.memory.buffer, ptr, W * H);
    const rows = new Array(H);
    for (let y = 0; y < H; y++)
      rows[y] = dec.decode(chars.subarray(y * W, (y + 1) * W));
    pre.textContent = rows.join("\n");

    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}

boot();
