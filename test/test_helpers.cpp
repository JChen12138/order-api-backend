#include "doctest.h"
#include "helpers.hpp"
#include "crow_all.h"
#include "../include/helpers.hpp"

crow::json::rvalue get_or_empty(crow::json::rvalue& obj, const std::string& key) {
    return obj.has(key) ? obj[key] : crow::json::rvalue();
}

TEST_CASE("is_valid_amount works") {
    crow::json::rvalue j_valid = crow::json::load(R"({"val": 9.99})");
    crow::json::rvalue j_zero = crow::json::load(R"({"val": 0})");
    crow::json::rvalue j_negative = crow::json::load(R"({"val": -5})");
    crow::json::rvalue j_str = crow::json::load(R"({"val": "100"})");

    auto valid = get_or_empty(j_valid, "val");
    auto zero = get_or_empty(j_zero, "val");
    auto negative = get_or_empty(j_negative, "val");
    auto str = get_or_empty(j_str, "val");

    CHECK(is_valid_amount(valid) == true);
    CHECK(is_valid_amount(zero) == false);
    CHECK(is_valid_amount(negative) == false);
    CHECK(is_valid_amount(str) == false);
}

TEST_CASE("is_valid_order_no works") {
    crow::json::rvalue j_ok = crow::json::load(R"({"no": "ORD12345"})");
    crow::json::rvalue j_empty = crow::json::load(R"({"no": ""})");
    crow::json::rvalue j_missing = crow::json::load(R"({})");
    crow::json::rvalue j_num = crow::json::load(R"({"no": 12345})");

    auto ok = get_or_empty(j_ok, "no");
    auto empty = get_or_empty(j_empty, "no");
    auto missing = get_or_empty(j_missing, "no");
    auto num = get_or_empty(j_num, "no");

    CHECK(is_valid_order_no(ok) == true);
    CHECK(is_valid_order_no(empty) == false);
    CHECK(is_valid_order_no(missing) == false);
    CHECK(is_valid_order_no(num) == false);
}

