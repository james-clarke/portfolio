const W = 160;
const H = 48;

async function boot() {
  const res = await fetch("waterfall.wasm");
  const { instance } = await WebAssembly.instantiate(await res.arrayBuffer());
  const e = instance.exports;

  if (e.wf_init(W, H, (Math.random() * 2 ** 32) >>> 0) !== 0)
    throw new Error("wf_init failed");

  const pre = document.getElementById("field");
  const dec = new TextDecoder();
  let prev = performance.now();

  function frame(t) {
    let dt = (t - prev) / 1000;
    prev = t;
    if (dt > 0.1) dt = 0.1;

    e.wf_step(dt);

    // re-view every frame
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
