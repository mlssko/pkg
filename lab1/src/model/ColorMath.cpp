#include "ColorMath.h"

#include <cmath>
#include <algorithm>

namespace ColorMath {

double clampValue(double v, double lo, double hi)
{
    return std::max(lo, std::min(hi, v));
}

RGB fitToGamut(RGB c, GamutStrategy strategy, bool& wasOutOfGamut)
{
    const double eps = 1e-9;
    wasOutOfGamut = (c.r < -eps || c.r > 1.0 + eps ||
                     c.g < -eps || c.g > 1.0 + eps ||
                     c.b < -eps || c.b > 1.0 + eps);

    if (!wasOutOfGamut) {
        return c;
    }

    if (strategy == GamutStrategy::Clipping) {
        c.r = clampValue(c.r, 0.0, 1.0);
        c.g = clampValue(c.g, 0.0, 1.0);
        c.b = clampValue(c.b, 0.0, 1.0);
        return c;
    }

    // Scaling: сначала убираем "уход в минус" сдвигом всего диапазона,
    // затем пропорционально сжимаем, если максимум больше 1.
    double minV = std::min({c.r, c.g, c.b});
    if (minV < 0.0) {
        c.r -= minV;
        c.g -= minV;
        c.b -= minV;
    }
    double maxV = std::max({c.r, c.g, c.b});
    if (maxV > 1.0) {
        double f = 1.0 / maxV;
        c.r *= f;
        c.g *= f;
        c.b *= f;
    }
    // Защитное финальное ограничение (на случай погрешностей округления).
    c.r = clampValue(c.r, 0.0, 1.0);
    c.g = clampValue(c.g, 0.0, 1.0);
    c.b = clampValue(c.b, 0.0, 1.0);
    return c;
}

// ============================================================================
// RGB <-> HLS
// ============================================================================

HLS rgbToHls(const RGB& c)
{
    double r = clampValue(c.r, 0.0, 1.0);
    double g = clampValue(c.g, 0.0, 1.0);
    double b = clampValue(c.b, 0.0, 1.0);

    double maxV = std::max({r, g, b});
    double minV = std::min({r, g, b});
    double delta = maxV - minV;

    HLS out;
    out.l = (maxV + minV) / 2.0;

    if (delta < 1e-9) {
        out.h = 0.0;
        out.s = 0.0;
        return out;
    }

    out.s = delta / (1.0 - std::fabs(2.0 * out.l - 1.0));

    double h;
    if (maxV == r) {
        h = 60.0 * std::fmod(((g - b) / delta), 6.0);
    } else if (maxV == g) {
        h = 60.0 * (((b - r) / delta) + 2.0);
    } else {
        h = 60.0 * (((r - g) / delta) + 4.0);
    }
    if (h < 0.0) h += 360.0;
    out.h = h;
    return out;
}

RGB hlsToRgb(const HLS& c, GamutStrategy strategy, bool& wasOutOfGamut)
{
    double h = c.h;
    while (h < 0.0) h += 360.0;
    h = std::fmod(h, 360.0);
    double l = c.l;
    double s = c.s;

    // Формула допускает выход исходных l/s за [0..1], если их ввели
    // вручную некорректно - тогда результат обработает fitToGamut().
    double cc = (1.0 - std::fabs(2.0 * l - 1.0)) * s;
    double x = cc * (1.0 - std::fabs(std::fmod(h / 60.0, 2.0) - 1.0));
    double m = l - cc / 2.0;

    double r1, g1, b1;
    if (h < 60.0)       { r1 = cc; g1 = x;  b1 = 0.0; }
    else if (h < 120.0) { r1 = x;  g1 = cc; b1 = 0.0; }
    else if (h < 180.0) { r1 = 0.0; g1 = cc; b1 = x;  }
    else if (h < 240.0) { r1 = 0.0; g1 = x;  b1 = cc; }
    else if (h < 300.0) { r1 = x;  g1 = 0.0; b1 = cc; }
    else                { r1 = cc; g1 = 0.0; b1 = x;  }

    RGB raw{ r1 + m, g1 + m, b1 + m };
    return fitToGamut(raw, strategy, wasOutOfGamut);
}

// ============================================================================
// RGB <-> CMYK  (GCR / UCR цветоделение)
// ============================================================================

CMYK rgbToCmyk(const RGB& c, CmykAlgorithm algorithm)
{
    double r = clampValue(c.r, 0.0, 1.0);
    double g = clampValue(c.g, 0.0, 1.0);
    double b = clampValue(c.b, 0.0, 1.0);

    double c0 = 1.0 - r;
    double m0 = 1.0 - g;
    double y0 = 1.0 - b;

    double rawK = std::min({c0, m0, y0});

    if (rawK >= 1.0 - 1e-9) {
        return CMYK{0.0, 0.0, 0.0, 1.0};
    }

    double k;
    if (algorithm == CmykAlgorithm::GCR) {
        // GCR: серая составляющая заменяется на чёрный по всему диапазону тонов.
        k = rawK;
    } else {
        // UCR: чёрный подмешивается преимущественно в тенях (квадратичная
        // зависимость: в светах K близко к 0, в тенях приближается к GCR).
        k = rawK * rawK;
    }

    double denom = 1.0 - k;
    CMYK out;
    out.k = k;
    out.c = clampValue((c0 - k) / denom, 0.0, 1.0);
    out.m = clampValue((m0 - k) / denom, 0.0, 1.0);
    out.y = clampValue((y0 - k) / denom, 0.0, 1.0);
    return out;
}

RGB cmykToRgb(const CMYK& c, GamutStrategy strategy, bool& wasOutOfGamut)
{
    double cc = c.c, mm = c.m, yy = c.y, kk = c.k;
    RGB raw;
    raw.r = (1.0 - cc) * (1.0 - kk);
    raw.g = (1.0 - mm) * (1.0 - kk);
    raw.b = (1.0 - yy) * (1.0 - kk);
    return fitToGamut(raw, strategy, wasOutOfGamut);
}

} // namespace ColorMath