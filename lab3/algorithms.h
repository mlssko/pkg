#pragma once
// Слой алгоритмов: чистые функции, принимают буфер и возвращают новый.
// Про интерфейс и Qt здесь ничего не известно.

#include <array>
#include <vector>
#include "pixelbuffer.h"

struct Histogram {
    std::array<int, 256> r{};
    std::array<int, 256> g{};
    std::array<int, 256> b{};
    std::array<int, 256> lum{}; // яркость Y = 0.299 R + 0.587 G + 0.114 B
};

namespace Algo {

// ---------- Сглаживающие фильтры ----------

// Ядра одномерные, размер 2r+1, сумма весов = 1
std::vector<float> makeBoxKernel(int radius);
std::vector<float> makeGaussKernel(int radius, double sigma);

// Свёртка двумя одномерными проходами: сначала по строкам, потом по столбцам, O(K * N^2)
ImageBuffer blurSeparable(const ImageBuffer& src, const std::vector<float>& kernel, EdgeMode mode);

// Обычная двумерная свёртка с ядром K x K, O(K^2 * N^2) - для сравнения скорости
ImageBuffer blurFull2D(const ImageBuffer& src, const std::vector<float>& kernel, EdgeMode mode);

// ---------- Гистограмма и контраст ----------

Histogram computeHistogram(const ImageBuffer& img);

// Линейное контрастирование по яркости; cutPercent - сколько процентов
// самых тёмных и самых светлых пикселей отбросить (0 = обычное растяжение min..max)
ImageBuffer linearContrast(const ImageBuffer& src, double cutPercent);

// Эквализация гистограммы отдельно по каналам R, G, B
ImageBuffer equalizeRGB(const ImageBuffer& src);

// Эквализация только компоненты V в пространстве HSV (H и S не меняются)
ImageBuffer equalizeHSV(const ImageBuffer& src);

// CLAHE по компоненте V в пространстве HSV
// tiles - число блоков по каждой стороне, clipLimit - порог отсечения (в долях от среднего значения столбца)
ImageBuffer clahe(const ImageBuffer& src, int tiles, double clipLimit);

}
