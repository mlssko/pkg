#pragma once
// Простейший parallelFor: делим диапазон строк на куски и раздаём потокам.

#include <algorithm>
#include <functional>
#include <thread>
#include <vector>

namespace Parallel {

inline int maxThreads()
{
    unsigned n = std::thread::hardware_concurrency();
    return n == 0 ? 1 : static_cast<int>(n);
}

inline int& threadCount()
{
    static int count = maxThreads();
    return count;
}

inline void setThreadCount(int n)
{
    threadCount() = std::max(1, n);
}

// body(from, to) обрабатывает строки [from, to)
inline void parallelFor(int begin, int end, const std::function<void(int, int)>& body)
{
    int total = end - begin;
    int n = threadCount();

    if (n <= 1 || total < 2 * n) {
        body(begin, end);
        return;
    }

    int chunk = (total + n - 1) / n;
    std::vector<std::thread> threads;
    for (int t = 0; t < n; ++t) {
        int a = begin + t * chunk;
        int b = std::min(end, a + chunk);
        if (a >= b)
            break;
        threads.emplace_back(body, a, b);
    }
    for (auto& th : threads)
        th.join();
}

} // namespace Parallel
