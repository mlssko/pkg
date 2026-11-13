const $ = id => document.getElementById(id);
const H = 25, G = new VirtualGrid(2 * H + 1, 2 * H + 1, H, H), P = new GridPainter($("cv"), G, H);
const HEAD = {
  stepLine: ["x", "y = k·x + b", "пиксель"], dda: ["i", "x; y (вещ.)", "пиксель"],
  bresenham: ["i", "ошибка e", "пиксель"], wu: ["x", "y; дробная часть", "пиксели: интенсивность"],
  circle: ["x", "d", "пиксель"], ellipse: ["шаг", "область, критерий d (×4)", "пиксель"]
};
const HINT = {
  stepLine: "Нужны x1, y1, x2, y2.", dda: "Нужны x1, y1, x2, y2.", bresenham: "Нужны x1, y1, x2, y2.", wu: "Нужны x1, y1, x2, y2.",
  circle: "Центр (x1, y1) и радиус r.", ellipse: "Центр (x1, y1) и полуоси a (по x), b (по y)."
};
function draw() {
  if (!$("acc").checked) G.clear();
  const v = id => Math.round(+$(id).value || 0), alg = $("alg").value, log = [];
  const x1 = v("x1"), y1 = v("y1"), x2 = v("x2"), y2 = v("y2"), r = Math.max(0, v("r")), a = Math.max(0, v("a")), b = Math.max(0, v("b"));
  let ext;
  if (alg === "circle") { RasterEngine.circle(x1, y1, r, G, log); ext = Math.max(Math.abs(x1) + r, Math.abs(y1) + r); }
  else if (alg === "ellipse") { RasterEngine.ellipse(x1, y1, a, b, G, log); ext = Math.max(Math.abs(x1) + a, Math.abs(y1) + b); }
  else { RasterEngine[alg](x1, y1, x2, y2, G, log); ext = Math.max(Math.abs(x1), Math.abs(y1), Math.abs(x2), Math.abs(y2)); }
  P.paint();
  let n = 0; for (const q of G.d) if (q > 0) n++;
  $("stat").textContent = "Закрашено клеток: " + n + (ext > H ? ". Часть фигуры вышла за поле ±" + H + " и обрезана." : ".");
  $("tinfo").textContent = $("alg").selectedOptions[0].text + ": " + (alg === "circle" ? "центр (" + x1 + ", " + y1 + "), r=" + r : alg === "ellipse" ? "центр (" + x1 + ", " + y1 + "), a=" + a + ", b=" + b : "(" + x1 + ", " + y1 + ") → (" + x2 + ", " + y2 + "), dx=" + (x2 - x1) + ", dy=" + (y2 - y1)) + ". Всего шагов: " + log.length + (log.length > 80 ? " (показаны первые 80)." : ".");
  $("trace").innerHTML = "<tr>" + HEAD[alg].map(h => "<th>" + h + "</th>").join("") + "</tr>" +
    log.slice(0, 80).map(rw => "<tr>" + rw.map(c => "<td>" + c + "</td>").join("") + "</tr>").join("");
}
$("draw").onclick = draw;
$("clr").onclick = () => { G.clear(); P.paint(); $("stat").textContent = "Растр очищен."; };
$("zoom").oninput = () => { P.c = +$("zoom").value; P.paint(); };
$("alg").onchange = () => { $("hint").textContent = HINT[$("alg").value]; };
$("cv").onmousemove = e => {
  const q = P.cellAt(e.offsetX, e.offsetY);
  if (q) $("stat").textContent = "Клетка (" + q[0] + ", " + q[1] + "), интенсивность " + G.d[(q[1] + H) * G.w + q[0] + H].toFixed(2);
};
$("hint").textContent = HINT.bresenham;
P.paint();


document.querySelectorAll(".tabs button").forEach(b => b.onclick = () => {
  document.querySelectorAll(".tabs button").forEach(x => x.classList.toggle("on", x === b));
  document.querySelectorAll("section").forEach(s => s.classList.toggle("on", s.id === b.dataset.t));
});
