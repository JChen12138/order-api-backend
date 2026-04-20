#include <string>
#include <sstream>
#include <chrono>
#include <utility>
#include <optional>
#include <ctime>

#include <sqlite3.h>
#include <sw/redis++/redis++.h>
#include <spdlog/spdlog.h>

#include "helpers.hpp"
#include "metrics.h"
#include "order_routes.h"
#include "service_state.h"

extern sqlite3* db;
extern sw::redis::Redis* redis;

using namespace std;
using namespace metrics;
using namespace service_state;

std::string format_time(time_t t);
std::string generate_order_no();
crow::response json_error(int code, const std::string& message);
bool is_valid_amount(const crow::json::rvalue& val);
bool is_valid_order_no(const crow::json::rvalue& val);

namespace {
void record_sqlite_failure(const string& message) {
    sqlite_errors.fetch_add(1, memory_order_relaxed);
    spdlog::error("{}", message);
}

void record_redis_failure(const string& message) {
    redis_errors.fetch_add(1, memory_order_relaxed);
    redis_available.store(false, memory_order_relaxed);
    spdlog::warn("{}", message);
}

void record_redis_success() {
    redis_available.store(true, memory_order_relaxed);
}

bool try_cache_order(const string& order_no, const string& payload) {
    if (redis == nullptr) {
        redis_errors.fetch_add(1, memory_order_relaxed);
        redis_available.store(false, memory_order_relaxed);
        spdlog::warn("Redis client not initialized. Skipping cache set for {}", order_no);
        return false;
    }

    try {
        redis->set("order:" + order_no, payload, chrono::seconds(300));
        record_redis_success();
        spdlog::info("Cached order {} in Redis (TTL: 300s)", order_no);
        return true;
    } catch (const sw::redis::Error& err) {
        record_redis_failure("Redis SET failed: " + string(err.what()));
        return false;
    }
}

optional<string> try_get_cached_order(const string& order_no) {
    if (redis == nullptr) {
        redis_errors.fetch_add(1, memory_order_relaxed);
        redis_available.store(false, memory_order_relaxed);
        return nullopt;
    }

    try {
        auto val = redis->get("order:" + order_no);
        record_redis_success();
        if (val) {
            cache_hits.fetch_add(1, memory_order_relaxed);
            spdlog::info("Redis cache hit for order: {}", order_no);
            return *val;
        }

        cache_misses.fetch_add(1, memory_order_relaxed);
        spdlog::info("Redis cache miss for order: {}", order_no);
        return nullopt;
    } catch (const sw::redis::Error& err) {
        cache_misses.fetch_add(1, memory_order_relaxed);
        record_redis_failure("Redis GET failed: " + string(err.what()));
        return nullopt;
    }
}

void invalidate_cached_order(const string& order_no, const char* action) {
    if (redis == nullptr) {
        redis_errors.fetch_add(1, memory_order_relaxed);
        redis_available.store(false, memory_order_relaxed);
        spdlog::warn("Redis client not initialized. Skipping cache invalidation for {}", order_no);
        return;
    }

    try {
        redis->del("order:" + order_no);
        record_redis_success();
        spdlog::info("{} {}", action, order_no);
    } catch (const sw::redis::Error& err) {
        record_redis_failure("Redis DEL failed: " + string(err.what()));
    }
}
}

crow::response create_order(const crow::request& req) {
    auto body = crow::json::load(req.body);
    if (!body) {
        return json_error(400, "Invalid JSON format");
    }
    if (!body.has("amount") || !is_valid_amount(body["amount"])) {
        return json_error(400, "Missing amount");
    }
    if (body["amount"].t() != crow::json::type::Number) {
        return json_error(400, "Amount must be a number");
    }

    const double amount = body["amount"].d();
    const string order_no = generate_order_no();
    const time_t now = time(nullptr);

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO orders (order_no, amount, status, created_at, paid_at) VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        record_sqlite_failure("Prepare failed: " + string(sqlite3_errmsg(db)));
        return json_error(500, "Internal DB error");
    }

    sqlite3_bind_text(stmt, 1, order_no.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, amount);
    sqlite3_bind_text(stmt, 3, "PENDING", -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_bind_int64(stmt, 5, 0);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        record_sqlite_failure("SQLite insert failed: " + string(sqlite3_errmsg(db)));
        sqlite3_finalize(stmt);
        return json_error(500, "Database error while creating order");
    }
    sqlite3_finalize(stmt);
    orders_created.fetch_add(1, memory_order_relaxed);

    crow::json::wvalue res;
    res["order_no"] = order_no;
    res["amount"] = amount;
    res["status"] = "PENDING";
    res["created_at"] = format_time(now);

    ostringstream oss;
    oss << res.dump();
    try_cache_order(order_no, oss.str());

    return crow::response(res);
}

crow::response get_order(const std::string& order_no) {
    if (auto cached = try_get_cached_order(order_no)) {
        return crow::response(*cached);
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT amount, status, created_at, paid_at FROM orders WHERE order_no = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        record_sqlite_failure("Prepare failed: " + string(sqlite3_errmsg(db)));
        return json_error(500, "Internal DB error");
    }
    sqlite3_bind_text(stmt, 1, order_no.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return json_error(404, "Order not found");
    }

    const double amount = sqlite3_column_double(stmt, 0);
    const string status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    const time_t created_at = sqlite3_column_int64(stmt, 2);
    const time_t paid_at = sqlite3_column_int64(stmt, 3);
    sqlite3_finalize(stmt);

    crow::json::wvalue res;
    res["order_no"] = order_no;
    res["amount"] = amount;
    res["status"] = status;
    res["created_at"] = format_time(created_at);
    res["paid_at"] = paid_at == 0 ? crow::json::wvalue() : format_time(paid_at);
    return crow::response(res);
}

crow::response pay_order(const crow::request& req) {
    auto body = crow::json::load(req.body);
    if (!body) {
        return json_error(400, "Invalid JSON format");
    }
    if (!body.has("order_no") || !is_valid_order_no(body["order_no"])) {
        return json_error(400, "Missing order_no");
    }
    if (body["order_no"].t() != crow::json::type::String) {
        return json_error(400, "order_no must be a string");
    }

    const string order_no = body["order_no"].s();
    sqlite3_stmt* stmt = nullptr;
    const char* select_sql = "SELECT amount, status, created_at FROM orders WHERE order_no = ?;";
    if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        record_sqlite_failure("Prepare failed: " + string(sqlite3_errmsg(db)));
        return json_error(500, "Internal DB error");
    }
    sqlite3_bind_text(stmt, 1, order_no.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return json_error(404, "Order not found");
    }

    const double amount = sqlite3_column_double(stmt, 0);
    const string status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    const time_t created_at = sqlite3_column_int64(stmt, 2);
    sqlite3_finalize(stmt);

    if (status == "PAID") {
        return json_error(400, "Already paid");
    }

    const time_t now = time(nullptr);
    const char* update_sql = "UPDATE orders SET status = 'PAID', paid_at = ? WHERE order_no = ?;";
    if (sqlite3_prepare_v2(db, update_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        record_sqlite_failure("Prepare failed: " + string(sqlite3_errmsg(db)));
        return json_error(500, "Internal DB error");
    }
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, order_no.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        record_sqlite_failure("SQLite update failed for payment: " + string(sqlite3_errmsg(db)));
        sqlite3_finalize(stmt);
        return json_error(500, "Failed to mark order as paid");
    }
    sqlite3_finalize(stmt);
    orders_paid.fetch_add(1, memory_order_relaxed);

    invalidate_cached_order(order_no, "Deleted order from Redis cache:");

    crow::json::wvalue res;
    res["order_no"] = order_no;
    res["status"] = "PAID";
    res["amount"] = amount;
    res["created_at"] = format_time(created_at);
    res["paid_at"] = format_time(now);
    return crow::response(res);
}

crow::response list_orders(const crow::request& req) {
    const string query = req.url_params.get("status") ? req.url_params.get("status") : "";
    const char* sql_all = "SELECT order_no, amount, status, created_at, paid_at FROM orders;";
    const char* sql_filtered = "SELECT order_no, amount, status, created_at, paid_at FROM orders WHERE status = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (query.empty()) {
        if (sqlite3_prepare_v2(db, sql_all, -1, &stmt, nullptr) != SQLITE_OK) {
            record_sqlite_failure("Prepare failed: " + string(sqlite3_errmsg(db)));
            return json_error(500, "Internal DB error");
        }
    } else {
        if (sqlite3_prepare_v2(db, sql_filtered, -1, &stmt, nullptr) != SQLITE_OK) {
            record_sqlite_failure("Prepare failed: " + string(sqlite3_errmsg(db)));
            return json_error(500, "Internal DB error");
        }
        sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_STATIC);
    }

    crow::json::wvalue result;
    auto& arr = result["orders"];
    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        crow::json::wvalue order;
        order["order_no"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        order["amount"] = sqlite3_column_double(stmt, 1);
        order["status"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        order["created_at"] = format_time(sqlite3_column_int64(stmt, 3));
        order["paid_at"] = sqlite3_column_int64(stmt, 4) == 0
            ? crow::json::wvalue()
            : format_time(sqlite3_column_int64(stmt, 4));
        arr[index++] = move(order);
    }
    sqlite3_finalize(stmt);

    return crow::response(result);
}

crow::response delete_order(const std::string& order_no) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM orders WHERE order_no = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        record_sqlite_failure("Prepare failed: " + string(sqlite3_errmsg(db)));
        return json_error(500, "Internal DB error");
    }
    sqlite3_bind_text(stmt, 1, order_no.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return json_error(404, "Order not found or could not delete");
    }

    const int deleted_rows = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    if (deleted_rows == 0) {
        return json_error(404, "Order not found");
    }

    invalidate_cached_order(order_no, "Deleted order from Redis:");

    crow::json::wvalue res;
    res["deleted"] = true;
    res["order_no"] = order_no;
    return crow::response(200, res);
}
