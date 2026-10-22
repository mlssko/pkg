#pragma once
// Слой доступа к памяти: обёртка над "сырыми" байтами изображения
// и обработка краёв (Clamp / Reflect). Qt здесь не используется.

#include <cstddef>
#include <vector>

enum class EdgeMode {
    Clamp,   // дублирование граничных пикселей: -1 -> 0, n -> n-1
    Reflect  // зеркальное отражение: -1 -> 0, -2 -> 1, n -> n-1, n+1 -> n-2
};

// Приводит индекс i к диапазону [0, n-1] по выбранной стратегии
int mapIndex(int i, int n, EdgeMode mode);

// Таблица для быстрого доступа: элемент k соответствует индексу i = k - r,
// то есть таблица покрывает диапазон [-r, n + r - 1]
std::vector<int> buildIndexMap(int n, int r, EdgeMode mode);

// Изображение: 3 канала (R, G, B), по 1 байту, строки идут подряд без выравнивания.
// Серые изображения хранятся как R = G = B.
class ImageBuffer
{
public:
    ImageBuffer() = default;
    ImageBuffer(int width, int height);

    int width() const { return m_width; }
    int height() const { return m_height; }
    bool isEmpty() const { return m_data.empty(); }

    unsigned char* row(int y) { return m_data.data() + static_cast<size_t>(y) * m_width * 3; }
    const unsigned char* row(int y) const { return m_data.data() + static_cast<size_t>(y) * m_width * 3; }

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<unsigned char> m_data;
};
