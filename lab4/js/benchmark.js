const BG = new VirtualGrid(1201, 1201, 600, 600);
const ALGS = [["Пошаговый", "stepLine", "#e11d48"], ["ЦДА", "dda", "#2563eb"], ["Брезенхем", "bresenham", "#16a34a"], ["Ву", "wu", "#d97706"]];
function rng(s) { return () => { s |= 0; s = s + 0x6D2B79F5 | 0; let t = Math.imul(s ^ s >>> 15, 1 | s); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }

function genLines(n, maxLen, fixed, seed) {
  const r = rng(seed), a = new Int32Array(2 * n);
  for (let i = 0; i < n; i++) {
    const ang = r() * 2 * Math.PI, L = fixed ? maxLen : 1 + r() * (maxLen - 1);
    a[2 * i] = Math.round(Math.cos(ang) * L); a[2 * i + 1] = Math.round(Math.sin(ang) * L);
  }
  return a;
}
function genCircles(n, seed) { 
  const r = rng(seed), a = new Int32Array(3 * n);
  for (let i = 0; i < n; i++) { a[3 * i] = Math.round(r() * 200 - 100); a[3 * i + 1] = Math.round(r() * 200 - 100); a[3 * i + 2] = 1 + Math.floor(r() * 250); }
  return a;
}
function timeLines(f, a, reps) {
  const n = a.length / 2; let best = 1e18;
  for (let k = 0; k < reps; k++) {
    BG.clear();  
    const t0 = performance.now();
    for (let i = 0; i < n; i++) f(0, 0, a[2 * i], a[2 * i + 1], BG, null);
    best = Math.min(best, performance.now() - t0);
  }
  return best;
}
function timeCircles(a, reps) {
  const n = a.length / 3; let best = 1e18;
  for (let k = 0; k < reps; k++) {
    BG.clear();
    const t0 = performance.now();
    for (let i = 0; i < n; i++) RasterEngine.circle(a[3 * i], a[3 * i + 1], a[3 * i + 2], BG, null);
    best = Math.min(best, performance.now() - t0);
  }
  return best;
}
const wait = () => new Promise(r => setTimeout(r, 0));

// график
function chart(title, xl, xs, series) {
  const cv = document.createElement("canvas"); cv.width = 460; cv.height = 300;
  $("charts").appendChild(cv);
  const x = cv.getContext("2d"), L = 55, B = 255, W = 385, Hh = 215;
  x.fillStyle = "#fff"; x.fillRect(0, 0, 460, 300);
  const xmax = Math.max(...xs), ymax = Math.max(...series.flatMap(s => s.ys)) * 1.05 || 1;
  const X = v => Math.round(L + v / xmax * W), Y = v => Math.round(B - v / ymax * Hh);
  x.fillStyle = "#222"; x.fillRect(L, B, W + 5, 1); x.fillRect(L, B - Hh - 5, 1, Hh + 5);
  x.font = "11px sans-serif"; x.textAlign = "center";
  for (let i = 0; i <= 5; i++) {
    const vx = xmax * i / 5, vy = ymax * i / 5;
    x.fillStyle = "#e3e5e8"; x.fillRect(X(vx), B - Hh, 1, Hh); x.fillRect(L, Y(vy), W, 1);
    x.fillStyle = "#222"; x.textAlign = "center"; x.fillText(Math.round(vx), X(vx), B + 14);
    x.textAlign = "right"; x.fillText(vy.toFixed(0), L - 4, Y(vy) + 4);
  }
  x.textAlign = "center"; x.font = "bold 12px sans-serif"; x.fillText(title, 230, 14);
  x.font = "11px sans-serif"; x.fillText(xl, L + W / 2, 290); x.fillText("мс", 18, 140);
  series.forEach((s, k) => {
    const tg = { setPixel: (px, py) => x.fillRect(px, py, 2, 2) };
    x.fillStyle = s.color;
    for (let i = 0; i < xs.length; i++) {
      if (i) RasterEngine.dda(X(xs[i - 1]), Y(s.ys[i - 1]), X(xs[i]), Y(s.ys[i]), tg);
      x.fillRect(X(xs[i]) - 2, Y(s.ys[i]) - 2, 5, 5);
    }
    x.fillRect(L + 10 + k * 85, 22, 10, 10); x.fillStyle = "#222"; x.textAlign = "left"; x.fillText(s.name, L + 24 + k * 85, 31);
  });
}
$("runb").onclick = async () => {
  const btn = $("runb"), st = $("bst"); btn.disabled = true; $("charts").innerHTML = "";
  st.textContent = "Прогрев…"; await wait();
  const warm = genLines(5000, 500, false, 1);
  ALGS.forEach(a => timeLines(RasterEngine[a[1]], warm, 2)); timeCircles(genCircles(2000, 1), 1);
  st.textContent = "100 000 отрезков и 50 000 окружностей…"; await wait();
  const L100 = genLines(100000, 500, false, 7), C50 = genCircles(50000, 9), tm = {};
  for (const a of ALGS) { tm[a[1]] = timeLines(RasterEngine[a[1]], L100, 3); await wait(); }
  const tc = timeCircles(C50, 3);
  $("btab").innerHTML = "<tr><th>Алгоритм</th><th>100 000 отрезков, мс</th><th>t / t(Брезенхем)</th></tr>" +
    ALGS.map(a => "<tr><td>" + a[0] + "</td><td>" + tm[a[1]].toFixed(1) + "</td><td>" + (tm[a[1]] / tm.bresenham).toFixed(2) + "</td></tr>").join("") +
    "<tr><td>Окружность Брезенхема (50 000 шт.)</td><td>" + tc.toFixed(1) + "</td><td>—</td></tr>";
  // график 1 время от числа отрезков
  st.textContent = "График 1…"; await wait();
  const cnt = [], s1 = ALGS.map(a => ({ name: a[0], color: a[2], ys: [] }));
  for (let n = 10000; n <= 100000; n += 10000) {
    cnt.push(n);
    for (let k = 0; k < ALGS.length; k++) s1[k].ys.push(timeLines(RasterEngine[ALGS[k][1]], L100.subarray(0, 2 * n), 2));
    await wait();
  }
  chart("Время от числа отрезков", "число отрезков", cnt, s1);
  // график 2 время от длины
  st.textContent = "График 2…"; await wait();
  const lens = [], s2 = ALGS.map(a => ({ name: a[0], color: a[2], ys: [] }));
  for (let len = 50; len <= 500; len += 50) {
    lens.push(len); const arr = genLines(20000, len, true, 3);
    for (let k = 0; k < ALGS.length; k++) s2[k].ys.push(timeLines(RasterEngine[ALGS[k][1]], arr, 2));
    await wait();
  }
  chart("Время от длины отрезка (20 000 шт.)", "длина, клеток", lens, s2);
  // график 3 окружности
  st.textContent = "График 3…"; await wait();
  const cc = [], s3 = [{ name: "Окружность Брезенхема", color: "#7c3aed", ys: [] }];
  for (let n = 5000; n <= 50000; n += 5000) { cc.push(n); s3[0].ys.push(timeCircles(C50.subarray(0, 3 * n), 2)); await wait(); }
  chart("Время от числа окружностей (r до 250)", "число окружностей", cc, s3);
  st.textContent = "Готово."; btn.disabled = false;
};
