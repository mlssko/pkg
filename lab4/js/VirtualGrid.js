class VirtualGrid {

  constructor(w, h, ox, oy) { this.w = w; this.h = h; this.ox = ox; this.oy = oy; this.d = new Float32Array(w * h); }
  setPixel(x, y, i) {
    x += this.ox; y += this.oy;
    if (x < 0 || y < 0 || x >= this.w || y >= this.h) return;
    const k = y * this.w + x;
    if (i > this.d[k]) this.d[k] = i;
  }
  clear() { this.d.fill(0); }
}
