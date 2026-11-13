// рисует закрашенные прямоугольники
class GridPainter {
  constructor(cv, grid, H) { this.cv = cv; this.g = grid; this.H = H; this.c = 16; this.M = 34; }
  paint() {
    const { cv, g, H, M } = this, c = this.c, N = 2 * H + 1, S = M * 2 + N * c;
    cv.width = cv.height = S;
    const x = cv.getContext("2d");
    x.fillStyle = "#fff"; x.fillRect(0, 0, S, S);
    for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {  // клетки с интенсивностью 
      const v = g.d[j * N + i];
      if (v > 0) { x.fillStyle = "rgba(37,99,235," + v + ")"; x.fillRect(M + i * c, M + (N - 1 - j) * c, c, c); }
    }
    x.fillStyle = "#d3d6db";                                        // линии сетки
    for (let k = 0; k <= N; k++) { x.fillRect(M + k * c, M, 1, N * c); x.fillRect(M, M + k * c, N * c, 1); }
    if (c >= 14) {                                                  // центры клеток = целые координаты
      x.fillStyle = "#8b9099";
      for (let i = 0; i < N; i++) for (let j = 0; j < N; j++) x.fillRect(M + i * c + c / 2 - 1, M + j * c + c / 2 - 1, 2, 2);
    }
    x.fillStyle = "#dc2626";     // оси
    x.fillRect(M + H * c + c / 2 - 1, M, 2, N * c); x.fillRect(M, M + H * c + c / 2 - 1, N * c, 2);
    x.fillStyle = "#222"; x.font = "11px sans-serif";
    const st = c >= 22 ? 1 : 5;
    for (let v = -H; v <= H; v++) if (v % st === 0) {
      x.textAlign = "center"; x.fillText(v, M + (v + H) * c + c / 2, M + N * c + 14);
      x.textAlign = "right"; x.fillText(v, M - 5, M + (H - v) * c + c / 2 + 4);
    }
    x.font = "bold 13px sans-serif"; x.textAlign = "left";
    x.fillText("x →", S - 28, M + N * c + 28); x.fillText("y ↑", 4, 16);
  }
  cellAt(px, py) {  
    const c = this.c, N = 2 * this.H + 1;
    const i = Math.floor((px - this.M) / c), j = Math.floor((py - this.M) / c);
    if (i < 0 || j < 0 || i >= N || j >= N) return null;
    return [i - this.H, this.H - j];
  }
}
