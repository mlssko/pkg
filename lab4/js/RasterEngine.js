const RasterEngine = {
  // Пошаговый алгоритм y = k*x + b
  stepLine(x1, y1, x2, y2, t, log) {
    const dx = x2 - x1, dy = y2 - y1;
    if (dx === 0 && dy === 0) { t.setPixel(x1, y1, 1); return; }
    if (Math.abs(dx) >= Math.abs(dy)) {
      const k = dy / dx, b = y1 - k * x1, s = dx > 0 ? 1 : -1;
      for (let x = x1; x !== x2 + s; x += s) {
        const y = k * x + b, r = Math.round(y);
        t.setPixel(x, r, 1);
        if (log) log.push([x, y.toFixed(3), "(" + x + ", " + r + ")"]);
      }
    } else { 
      const k = dx / dy, b = x1 - k * y1, s = dy > 0 ? 1 : -1;
      for (let y = y1; y !== y2 + s; y += s) {
        const x = k * y + b, r = Math.round(x);
        t.setPixel(r, y, 1);
        if (log) log.push([y, x.toFixed(3), "(" + r + ", " + y + ")"]);
      }
    }
  },
  // ЦДА
  dda(x1, y1, x2, y2, t, log) {
    const dx = x2 - x1, dy = y2 - y1, n = Math.max(Math.abs(dx), Math.abs(dy));
    if (n === 0) { t.setPixel(x1, y1, 1); return; }
    const sx = dx / n, sy = dy / n;
    let x = x1, y = y1;
    for (let i = 0; i <= n; i++) {
      const rx = Math.round(x), ry = Math.round(y);
      t.setPixel(rx, ry, 1);
      if (log) log.push([i, x.toFixed(3) + "; " + y.toFixed(3), "(" + rx + ", " + ry + ")"]);
      x += sx; y += sy;
    }
  },
  // Брезенхем, обобщённый на все 8 октантов
  bresenham(x1, y1, x2, y2, t, log) {
    const dx = Math.abs(x2 - x1), dy = Math.abs(y2 - y1);
    const sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    let x = x1, y = y1;
    if (dx >= dy) {  
      let e = 2 * dy - dx;
      for (let i = 0; i <= dx; i++) {
        t.setPixel(x, y, 1);
        if (log) log.push([i, e, "(" + x + ", " + y + ")"]);
        if (e >= 0) { y += sy; e -= 2 * dx; }
        x += sx; e += 2 * dy;
      }
    } else { 
      let e = 2 * dx - dy;
      for (let i = 0; i <= dy; i++) {
        t.setPixel(x, y, 1);
        if (log) log.push([i, e, "(" + x + ", " + y + ")"]);
        if (e >= 0) { x += sx; e -= 2 * dy; }
        y += sy; e += 2 * dx;
      }
    }
  },
  // Ву: интенсивность двух соседних пикселей определяется дробной частью координаты
  wu(x1, y1, x2, y2, t, log) {
    const steep = Math.abs(y2 - y1) > Math.abs(x2 - x1);
    if (steep) { [x1, y1] = [y1, x1]; [x2, y2] = [y2, x2]; }
    if (x1 > x2) { [x1, x2] = [x2, x1]; [y1, y2] = [y2, y1]; }
    const dx = x2 - x1, g = dx === 0 ? 0 : (y2 - y1) / dx;
    let y = y1;
    for (let x = x1; x <= x2; x++) {
      const yi = Math.floor(y), f = y - yi; 
      if (steep) { t.setPixel(yi, x, 1 - f); if (f > 1e-9) t.setPixel(yi + 1, x, f); }
      else { t.setPixel(x, yi, 1 - f); if (f > 1e-9) t.setPixel(x, yi + 1, f); }
      if (log) {
        const p = steep ? (a, b) => "(" + b + ", " + a + ")" : (a, b) => "(" + a + ", " + b + ")";
        log.push([x, y.toFixed(3) + "; f=" + f.toFixed(3), p(x, yi) + ": " + (1 - f).toFixed(2) + ";  " + p(x, yi + 1) + ": " + f.toFixed(2)]);
      }
      y += g;
    }
  },
  // Окружность Брезенхема
  circle(cx, cy, r, t, log) {
    let x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
      t.setPixel(cx + x, cy + y, 1); t.setPixel(cx - x, cy + y, 1); t.setPixel(cx + x, cy - y, 1); t.setPixel(cx - x, cy - y, 1);
      t.setPixel(cx + y, cy + x, 1); t.setPixel(cx - y, cy + x, 1); t.setPixel(cx + y, cy - x, 1); t.setPixel(cx - y, cy - x, 1);
      if (log) log.push([x, d, "(" + x + ", " + y + ") и 7 симметричных"]);
      if (d < 0) d += 4 * x + 6; else { d += 4 * (x - y) + 10; y--; }
      x++;
    }
  },
  // Кастл-Питвей для эллипса
  ellipse(cx, cy, a, b, t, log) {
    const a2 = a * a, b2 = b * b;
    const p4 = (x, y, n) => {
      t.setPixel(cx + x, cy + y, 1); t.setPixel(cx - x, cy + y, 1); t.setPixel(cx + x, cy - y, 1); t.setPixel(cx - x, cy - y, 1);
      if (log) log.push([n, "", "(" + x + ", " + y + ") и 3 симметричных"]);
    };
    let x = 0, y = b, dx = 0, dy = 2 * a2 * b, n = 0;
    let d = 4 * b2 - 4 * a2 * b + a2;   
    while (dx < dy) {  
      p4(x, y, n++); if (log) log[log.length - 1][1] = "обл.1, d=" + d;
      x++; dx += 2 * b2;
      if (d < 0) d += 4 * (dx + b2); else { y--; dy -= 2 * a2; d += 4 * (dx - dy + b2); }
    }
    d = b2 * (2 * x + 1) * (2 * x + 1) + 4 * a2 * (y - 1) * (y - 1) - 4 * a2 * b2; 
    while (y >= 0) {
      p4(x, y, n++); if (log) log[log.length - 1][1] = "обл.2, d=" + d;
      y--; dy -= 2 * a2;
      if (d > 0) d += 4 * (a2 - dy); else { x++; dx += 2 * b2; d += 4 * (dx - dy + a2); }
    }
  }
};
