#pragma once

#include <atomic>
#include <string>

namespace runtime_config {
    inline std::string api_key = "1234567";
    inline std::atomic<int> cache_ttl_seconds{300};
}
