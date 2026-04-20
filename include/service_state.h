#pragma once

#include <atomic>
#include <string>

namespace service_state {
    inline std::atomic<bool> shutting_down{false};
    inline std::atomic<bool> db_ready{false};
    inline std::atomic<bool> redis_available{false};
    inline std::atomic<int> max_inflight_requests{64};

    inline bool is_probe_path(const std::string& path) {
        return path == "/healthcheck" || path == "/readiness" || path == "/metrics";
    }

    inline bool is_ready() {
        return db_ready.load(std::memory_order_relaxed) &&
               !shutting_down.load(std::memory_order_relaxed);
    }

    inline const char* readiness_status() {
        if (!db_ready.load(std::memory_order_relaxed)) {
            return "db_unavailable";
        }
        if (shutting_down.load(std::memory_order_relaxed)) {
            return "shutting_down";
        }
        if (!redis_available.load(std::memory_order_relaxed)) {
            return "degraded";
        }
        return "ready";
    }
}
