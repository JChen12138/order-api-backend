#pragma once
#include <atomic>
#include <algorithm>
#include <cstdint>

namespace metrics {
    inline std::atomic<int> total_requests{0};
    inline std::atomic<int> orders_created{0};
    inline std::atomic<int> orders_paid{0};
    inline std::atomic<int> cache_hits{0};
    inline std::atomic<int> cache_misses{0};
    inline std::atomic<int> overload_rejections{0};
    inline std::atomic<int> shutdown_rejections{0};
    inline std::atomic<int> redis_errors{0};
    inline std::atomic<int> sqlite_errors{0};
    inline std::atomic<int> in_flight_requests{0};
    inline std::atomic<int64_t> request_duration_ms_total{0};
    inline std::atomic<int64_t> request_duration_ms_max{0};
    inline std::atomic<int64_t> request_duration_samples{0};

    inline void observe_request_duration_ms(int64_t duration_ms) {
        request_duration_ms_total.fetch_add(duration_ms, std::memory_order_relaxed);
        request_duration_samples.fetch_add(1, std::memory_order_relaxed);

        auto current_max = request_duration_ms_max.load(std::memory_order_relaxed);
        while (duration_ms > current_max && !request_duration_ms_max.compare_exchange_weak(//compare_exchange_weak: update this atomic only if it still equals the expected value
                   current_max, duration_ms, std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
    }
}
