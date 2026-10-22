#include "pixelbuffer.h"

int mapIndex(int i, int n, EdgeMode mode)
{
    if (n <= 1)
        return 0;

    if (mode == EdgeMode::Clamp) {
        if (i < 0)
            return 0;
        if (i >= n)
            return n - 1;
        return i;
    }

    // Reflect: отражаем, пока индекс не попадёт в диапазон
    // (цикл нужен, если радиус больше размера изображения)
    while (i < 0 || i >= n) {
        if (i < 0)
            i = -i - 1;
        else
            i = 2 * n - 1 - i;
    }
    return i;
}

std::vector<int> buildIndexMap(int n, int r, EdgeMode mode)
{
    std::vector<int> map(n + 2 * r);
    for (int k = 0; k < n + 2 * r; ++k)
        map[k] = mapIndex(k - r, n, mode);
    return map;
}

ImageBuffer::ImageBuffer(int width, int height)
    : m_width(width), m_height(height)
{
    m_data.assign(static_cast<size_t>(width) * height * 3, 0);
}
