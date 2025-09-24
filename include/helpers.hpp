#pragma once
//include guard that tells the compiler:“Only include this file once in a single compilation unit, even if it’s #included multiple times.”
#include "crow_all.h"
#include <string>

inline bool is_valid_amount(const crow::json::rvalue& val) {
    return val.t() == crow::json::type::Number && val.d() > 0;
}


bool is_valid_order_no(const crow::json::rvalue& val) {
    if (!val) return false; // Protect against invalid/null object

    try {
        if (val.t() != crow::json::type::String)
            return false;

        std::string s = val.s();
        return !s.empty() && s.substr(0, 3) == "ORD";
    } catch (...) {
        return false; // Safeguard any strange exceptions
    }
}




std::string generate_order_no();