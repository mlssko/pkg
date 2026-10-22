#include "algorithms.h"
#include "parallel.h"

#include <algorithm>
#include <cmath>

namespace {

unsigned char toByte(float v)
{
    if (v < 0.f)
        return 0;
    if (v > 255.f)
        return 255;
    return static_cast<unsigned char>(v + 0.5f);
}

using Lut = std::array<unsigned char, 256>;

// Таблица эквализации: s = (cdf - cdfMin) / (N - cdfMin) * 255
Lut buildEqualizeLut(const std::array<int, 256>& hist, long long total)
{
    Lut lut;
    long long cdf[256];
    long long sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += hist[i];
        cdf[i] = sum;
    }

    long long cdfMin = 0;
    for (int i = 0; i < 256; ++i) {
        if (cdf[i] > 0) {
            cdfMin = cdf[i];
            break;
        }
    }

    for (int i = 0; i < 256; ++i) {
        if (total == cdfMin) {
            lut[i] = static_cast<unsigned char>(i); // изображение одноцветное
        } else {
            double v = double(cdf[i] - cdfMin) * 255.0 / double(total - cdfMin);
            lut[i] = toByte(static_cast<float>(std::max(0.0, v)));
        }
    }
    return lut;
}

// Применяет свои таблицы к каждому каналу
ImageBuffer applyLut(const ImageBuffer& src, const Lut& lr, const Lut& lg, const Lut& lb)
{
    int w = src.width();
    int h = src.height();
    ImageBuffer out(w, h);

    Parallel::parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const unsigned char* s = src.row(y);
            unsigned char* d = out.row(y);
            for (int x = 0; x < w; ++x) {
                d[3 * x]     = lr[s[3 * x]];
                d[3 * x + 1] = lg[s[3 * x + 1]];
                d[3 * x + 2] = lb[s[3 * x + 2]];
            }
        }
    });
    return out;
}

// ---------- RGB <-> HSV ----------

void rgbToHsv(unsigned char R, unsigned char G, unsigned char B, float& h, float& s, float& v)
{
    float r = R / 255.f, g = G / 255.f, b = B / 255.f;
    float mx = std::max(r, std::max(g, b));
    float mn = std::min(r, std::min(g, b));
    float d = mx - mn;

    v = mx;
    s = (mx == 0.f) ? 0.f : d / mx;

    if (d == 0.f)
        h = 0.f;
    else if (mx == r)
        h = 60.f * std::fmod((g - b) / d, 6.f);
    else if (mx == g)
        h = 60.f * ((b - r) / d + 2.f);
    else
        h = 60.f * ((r - g) / d + 4.f);

    if (h < 0.f)
        h += 360.f;
}

void hsvToRgb(float h, float s, float v, unsigned char& R, unsigned char& G, unsigned char& B)
{
    float c = v * s;
    float hp = h / 60.f;
    float x = c * (1.f - std::fabs(std::fmod(hp, 2.f) - 1.f));
    float m = v - c;

    float r = 0, g = 0, b = 0;
    switch (static_cast<int>(hp) % 6) {
    case 0: r = c; g = x; b = 0; break;
    case 1: r = x; g = c; b = 0; break;
    case 2: r = 0; g = c; b = x; break;
    case 3: r = 0; g = x; b = c; break;
    case 4: r = x; g = 0; b = c; break;
    default: r = c; g = 0; b = x; break;
    }

    R = toByte((r + m) * 255.f);
    G = toByte((g + m) * 255.f);
    B = toByte((b + m) * 255.f);
}

// Изображение, разложенное на плоскости H, S, V
struct HsvPlanes {
    int w = 0;
    int h = 0;
    std::vector<float> H;
    std::vector<float> S;
    std::vector<unsigned char> V; // 0..255
};

HsvPlanes splitHsv(const ImageBuffer& src)
{
    HsvPlanes p;
    p.w = src.width();
    p.h = src.height();
    size_t n = static_cast<size_t>(p.w) * p.h;
    p.H.resize(n);
    p.S.resize(n);
    p.V.resize(n);

    Parallel::parallelFor(0, p.h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const unsigned char* s = src.row(y);
            for (int x = 0; x < p.w; ++x) {
                float h, sat, v;
                rgbToHsv(s[3 * x], s[3 * x + 1], s[3 * x + 2], h, sat, v);
                size_t i = static_cast<size_t>(y) * p.w + x;
                p.H[i] = h;
                p.S[i] = sat;
                p.V[i] = toByte(v * 255.f);
            }
        }
    });
    return p;
}

ImageBuffer mergeHsv(const HsvPlanes& p, const std::vector<unsigned char>& newV)
{
    ImageBuffer out(p.w, p.h);

    Parallel::parallelFor(0, p.h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            unsigned char* d = out.row(y);
            for (int x = 0; x < p.w; ++x) {
                size_t i = static_cast<size_t>(y) * p.w + x;
                hsvToRgb(p.H[i], p.S[i], newV[i] / 255.f, d[3 * x], d[3 * x + 1], d[3 * x + 2]);
            }
        }
    });
    return out;
}

// CLAHE для одной плоскости яркости
std::vector<unsigned char> claheOnPlane(const std::vector<unsigned char>& plane,
                                        int w, int h, int tiles, double clipLimit)
{
    int tx = std::max(1, std::min(tiles, w));
    int ty = std::max(1, std::min(tiles, h));
    double tw = double(w) / tx;
    double th = double(h) / ty;

    // 1. гистограммы по блокам
    std::vector<std::array<int, 256>> hist(static_cast<size_t>(tx) * ty);
    for (auto& hh : hist)
        hh.fill(0);
    std::vector<int> count(static_cast<size_t>(tx) * ty, 0);

    for (int y = 0; y < h; ++y) {
        int by = std::min(ty - 1, static_cast<int>(y / th));
        for (int x = 0; x < w; ++x) {
            int bx = std::min(tx - 1, static_cast<int>(x / tw));
            int t = by * tx + bx;
            hist[t][plane[static_cast<size_t>(y) * w + x]]++;
            count[t]++;
        }
    }

    // 2. отсечение гистограммы и таблица для каждого блока
    std::vector<Lut> luts(static_cast<size_t>(tx) * ty);
    for (int t = 0; t < tx * ty; ++t) {
        auto& hh = hist[t];
        int total = count[t];

        int limit = std::max(1, static_cast<int>(clipLimit * total / 256.0));
        int excess = 0;
        for (int i = 0; i < 256; ++i) {
            if (hh[i] > limit) {
                excess += hh[i] - limit;
                hh[i] = limit;
            }
        }

        // отсечённое равномерно раздаём всем столбцам
        int add = excess / 256;
        int rest = excess % 256;
        for (int i = 0; i < 256; ++i)
            hh[i] += add;
        if (rest > 0) {
            int step = std::max(1, 256 / rest);
            for (int i = 0; i < 256 && rest > 0; i += step, --rest)
                hh[i]++;
        }

        long long cdf = 0;
        for (int i = 0; i < 256; ++i) {
            cdf += hh[i];
            luts[t][i] = toByte(static_cast<float>(cdf * 255.0 / total));
        }
    }

    // 3. каждый пиксель - билинейная интерполяция таблиц четырёх ближайших блоков
    std::vector<unsigned char> out(plane.size());
    Parallel::parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            double fy = y / th - 0.5;
            int iy0 = static_cast<int>(std::floor(fy));
            double ay = fy - iy0;
            int ya = std::clamp(iy0, 0, ty - 1);
            int yb = std::clamp(iy0 + 1, 0, ty - 1);

            for (int x = 0; x < w; ++x) {
                double fx = x / tw - 0.5;
                int ix0 = static_cast<int>(std::floor(fx));
                double ax = fx - ix0;
                int xa = std::clamp(ix0, 0, tx - 1);
                int xb = std::clamp(ix0 + 1, 0, tx - 1);

                int v = plane[static_cast<size_t>(y) * w + x];
                double top = (1 - ax) * luts[ya * tx + xa][v] + ax * luts[ya * tx + xb][v];
                double bottom = (1 - ax) * luts[yb * tx + xa][v] + ax * luts[yb * tx + xb][v];
                double res = (1 - ay) * top + ay * bottom;
                out[static_cast<size_t>(y) * w + x] = toByte(static_cast<float>(res));
            }
        }
    });
    return out;
}

} // namespace

// =====================================================================
//                      Сглаживающие фильтры
// =====================================================================

std::vector<float> Algo::makeBoxKernel(int radius)
{
    int size = 2 * radius + 1;
    return std::vector<float>(size, 1.f / size);
}

std::vector<float> Algo::makeGaussKernel(int radius, double sigma)
{
    std::vector<float> k(2 * radius + 1);
    double sum = 0;
    for (int i = -radius; i <= radius; ++i) {
        double v = std::exp(-(i * i) / (2.0 * sigma * sigma));
        k[i + radius] = static_cast<float>(v);
        sum += v;
    }
    for (auto& v : k)
        v = static_cast<float>(v / sum);
    return k;
}

ImageBuffer Algo::blurSeparable(const ImageBuffer& src, const std::vector<float>& kernel, EdgeMode mode)
{
    int w = src.width();
    int h = src.height();
    int size = static_cast<int>(kernel.size());
    int r = size / 2;

    ImageBuffer out(w, h);
    std::vector<float> tmp(static_cast<size_t>(w) * h * 3);

    std::vector<int> xmap = buildIndexMap(w, r, mode);
    std::vector<int> ymap = buildIndexMap(h, r, mode);

    // проход 1: по строкам, результат во float, чтобы не копить ошибку округления
    Parallel::parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const unsigned char* s = src.row(y);
            float* t = tmp.data() + static_cast<size_t>(y) * w * 3;
            for (int x = 0; x < w; ++x) {
                float sr = 0, sg = 0, sb = 0;
                for (int i = 0; i < size; ++i) {
                    const unsigned char* p = s + 3 * xmap[x + i];
                    float k = kernel[i];
                    sr += k * p[0];
                    sg += k * p[1];
                    sb += k * p[2];
                }
                t[3 * x]     = sr;
                t[3 * x + 1] = sg;
                t[3 * x + 2] = sb;
            }
        }
    });

    // проход 2: по столбцам. Идём строками, чтобы память читалась подряд
    Parallel::parallelFor(0, h, [&](int y0, int y1) {
        std::vector<float> acc(static_cast<size_t>(w) * 3);
        for (int y = y0; y < y1; ++y) {
            std::fill(acc.begin(), acc.end(), 0.f);
            for (int i = 0; i < size; ++i) {
                const float* t = tmp.data() + static_cast<size_t>(ymap[y + i]) * w * 3;
                float k = kernel[i];
                for (int j = 0; j < w * 3; ++j)
                    acc[j] += k * t[j];
            }
            unsigned char* d = out.row(y);
            for (int j = 0; j < w * 3; ++j)
                d[j] = toByte(acc[j]);
        }
    });

    return out;
}

ImageBuffer Algo::blurFull2D(const ImageBuffer& src, const std::vector<float>& kernel, EdgeMode mode)
{
    int w = src.width();
    int h = src.height();
    int size = static_cast<int>(kernel.size());
    int r = size / 2;

    ImageBuffer out(w, h);
    std::vector<int> xmap = buildIndexMap(w, r, mode);
    std::vector<int> ymap = buildIndexMap(h, r, mode);

    Parallel::parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            unsigned char* d = out.row(y);
            for (int x = 0; x < w; ++x) {
                float sr = 0, sg = 0, sb = 0;
                for (int i = 0; i < size; ++i) {
                    const unsigned char* line = src.row(ymap[y + i]);
                    for (int j = 0; j < size; ++j) {
                        const unsigned char* p = line + 3 * xmap[x + j];
                        float k = kernel[i] * kernel[j]; // двумерное ядро = произведение одномерных
                        sr += k * p[0];
                        sg += k * p[1];
                        sb += k * p[2];
                    }
                }
                d[3 * x]     = toByte(sr);
                d[3 * x + 1] = toByte(sg);
                d[3 * x + 2] = toByte(sb);
            }
        }
    });
    return out;
}

// =====================================================================
//                      Гистограмма и контраст
// =====================================================================

Histogram Algo::computeHistogram(const ImageBuffer& img)
{
    Histogram hist;
    for (int y = 0; y < img.height(); ++y) {
        const unsigned char* p = img.row(y);
        for (int x = 0; x < img.width(); ++x) {
            int r = p[3 * x], g = p[3 * x + 1], b = p[3 * x + 2];
            hist.r[r]++;
            hist.g[g]++;
            hist.b[b]++;
            hist.lum[(299 * r + 587 * g + 114 * b + 500) / 1000]++;
        }
    }
    return hist;
}

ImageBuffer Algo::linearContrast(const ImageBuffer& src, double cutPercent)
{
    Histogram hist = computeHistogram(src);
    long long total = static_cast<long long>(src.width()) * src.height();
    long long cut = static_cast<long long>(total * cutPercent / 100.0);

    // нижняя граница: первый уровень, где накопленная сумма превысила cut
    int lo = 0;
    long long cum = 0;
    for (; lo < 255; ++lo) {
        cum += hist.lum[lo];
        if (cum > cut)
            break;
    }
    // верхняя граница: то же самое, но с конца
    int hi = 255;
    cum = 0;
    for (; hi > 0; --hi) {
        cum += hist.lum[hi];
        if (cum > cut)
            break;
    }

    if (hi <= lo)
        return src; // растягивать нечего

    // y = (x - lo) * 255 / (hi - lo), одна и та же функция для всех каналов - цвета не "уплывают"
    Lut lut;
    for (int i = 0; i < 256; ++i) {
        double v = (i - lo) * 255.0 / (hi - lo);
        lut[i] = toByte(static_cast<float>(std::min(255.0, std::max(0.0, v))));
    }
    return applyLut(src, lut, lut, lut);
}

ImageBuffer Algo::equalizeRGB(const ImageBuffer& src)
{
    Histogram hist = computeHistogram(src);
    long long total = static_cast<long long>(src.width()) * src.height();
    Lut lr = buildEqualizeLut(hist.r, total);
    Lut lg = buildEqualizeLut(hist.g, total);
    Lut lb = buildEqualizeLut(hist.b, total);
    return applyLut(src, lr, lg, lb);
}

ImageBuffer Algo::equalizeHSV(const ImageBuffer& src)
{
    HsvPlanes p = splitHsv(src);

    std::array<int, 256> hist{};
    for (unsigned char v : p.V)
        hist[v]++;
    Lut lut = buildEqualizeLut(hist, static_cast<long long>(p.V.size()));

    std::vector<unsigned char> newV(p.V.size());
    for (size_t i = 0; i < p.V.size(); ++i)
        newV[i] = lut[p.V[i]];

    return mergeHsv(p, newV);
}

ImageBuffer Algo::clahe(const ImageBuffer& src, int tiles, double clipLimit)
{
    HsvPlanes p = splitHsv(src);
    std::vector<unsigned char> newV = claheOnPlane(p.V, p.w, p.h, tiles, clipLimit);
    return mergeHsv(p, newV);
}
