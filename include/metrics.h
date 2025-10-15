#pragma once
#include <atomic>

namespace metrics {
    inline std::atomic<int> total_requests{0};
    inline std::atomic<int> orders_created{0};
    inline std::atomic<int> orders_paid{0};
    inline std::atomic<int> cache_hits{0};
    inline std::atomic<int> cache_misses{0};
}
