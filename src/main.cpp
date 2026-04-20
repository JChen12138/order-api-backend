#include "crow_all.h"
#include <ctime>
#include <string>
#include <iostream>
#include <algorithm>
#include <thread>
#include <csignal>
#include <memory>
#include <sqlite3.h>
#include <sw/redis++/redis++.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <cstdlib>
#include "order_routes.h"
#include "auth_middleware.h"
#include "metrics.h"
#include "service_state.h"

using namespace sw::redis;
using namespace std;
using namespace metrics;
using namespace service_state;

string get_env(const string& var, const string& default_val) {
    const char* val = getenv(var.c_str());
    return val ? string(val) : default_val;
}

inline bool is_valid_amount(const crow::json::rvalue& val) {
    return val.t() == crow::json::type::Number && val.d() > 0;
}

inline bool is_valid_order_no(const crow::json::rvalue& val) {
    return val.t() == crow::json::type::String && val.s().size() > 0;
}
//inline keyword in C++ is a compiler suggestion to insert the function’s body directly at each call site, 
//instead of calling it via the normal function-call mechanism. It’s often used for small, frequently used


crow::response json_error(int code, const std::string& message) {
    crow::json::wvalue err;
    err["error"] = message;
    return crow::response(code, err);
}

sqlite3* db = nullptr;
Redis* redis = nullptr;
namespace { //everything inside is only visible in this .cpp file
    unique_ptr<Redis> redis_owner;
    atomic<bool> shutdown_signal_received{false};

    void signal_handler(int /*signal_number*/) {
        shutdown_signal_received.store(true, memory_order_relaxed);
    }
}

struct ErrorHandlerMiddleware {
    struct context {}; // Required even if unused

    void before_handle(crow::request& req, crow::response& res, context& ctx) {
        // Do nothing before request
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        if (res.code >= 400) {
            // Log the error
            spdlog::warn("Error {} on {} {}", res.code, crow::method_name(req.method), req.url);

            // If response body is empty, fill with JSON error
            if (res.body.empty()) {
                std::string error_msg;

                switch (res.code) {
                    case 400: error_msg = "Bad Request"; break;
                    case 404: error_msg = "Not Found"; break;
                    case 500: error_msg = "Internal Server Error"; break;
                    default:  error_msg = "HTTP Error"; break;
                }

                crow::json::wvalue json;
                json["error"] = error_msg;
                json["code"] = res.code;
                res.set_header("Content-Type", "application/json");
                res.body = json.dump(); 
            }
        }
    }
};

struct LifecycleMiddleware {
    struct context {
        bool counted_inflight = false;
    };

    void before_handle(crow::request& req, crow::response& res, context& ctx) {
        if (is_probe_path(req.url)) {
            return;
        }

        if (shutting_down.load(memory_order_relaxed)) {
            shutdown_rejections.fetch_add(1, memory_order_relaxed);
            res = json_error(503, "Server is shutting down");
            res.set_header("Retry-After", "5");
            res.end();
            return;
        }

        const int current_inflight = in_flight_requests.fetch_add(1, memory_order_relaxed) + 1;
        ctx.counted_inflight = true;
        if (current_inflight > max_inflight_requests.load(memory_order_relaxed)) {
            in_flight_requests.fetch_sub(1, memory_order_relaxed);
            ctx.counted_inflight = false;
            overload_rejections.fetch_add(1, memory_order_relaxed);
            res = json_error(503, "Server overloaded");
            res.set_header("Retry-After", "1");
            res.end();
        }
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        if (ctx.counted_inflight) {
            in_flight_requests.fetch_sub(1, memory_order_relaxed);
        }
    }
};

struct LoggingMiddleware {
    struct context {
        chrono::steady_clock::time_point start_time;
    };

    void before_handle(crow::request& req, crow::response& res, context& ctx) {
    //Crow calls this function internally as part of its request lifecycle, and it expects all three parameters to be there.
        ctx.start_time = chrono::steady_clock::now();
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - ctx.start_time).count();
        total_requests.fetch_add(1, memory_order_relaxed);
        observe_request_duration_ms(duration);

        spdlog::info("{} {} {} ({} ms)", crow::method_name(req.method), req.url, res.code, duration);

    }
};

string generate_order_no(){
    long long timestamp = time(nullptr);
    int rand_part = rand()%100000;
    return "ORD" + to_string(timestamp) + to_string(rand_part);
}

string format_time(time_t t){
    if(t == 0) return "N/A";
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return string(buf);
}

void init_db(){
    if(sqlite3_open("orders.db", &db)){
        //cerr << "Can't open DB: " << sqlite3_errmsg(db) << endl;
        spdlog::error("Can't open DB: {}", sqlite3_errmsg(db));
        db_ready.store(false, memory_order_relaxed);
        exit(1);
    }
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS orders(
            order_no TEXT PRIMARY KEY,
            amount REAL,
            status TEXT,
            created_at INTEGER,
            paid_at INTEGER
        );
    )";
    char* errMsg = nullptr;
    if(sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK){
        //cerr << "SQL error: " << errMsg << endl;
        spdlog::error("SQL error: {}", errMsg);
        sqlite3_free(errMsg);
        db_ready.store(false, memory_order_relaxed);
        exit(1);
    }
    db_ready.store(true, memory_order_relaxed);
}

int main(){
    srand(time(nullptr));

    auto logger = spdlog::basic_logger_mt("file_logger", "logs/server.log");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info); // Or debug
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");

    init_db();

    max_inflight_requests.store(
        max(1, stoi(get_env("MAX_INFLIGHT_REQUESTS", "64"))),
        memory_order_relaxed);

    string redis_host = get_env("REDIS_HOST", "127.0.0.1");
    try {
        redis_owner = make_unique<Redis>("tcp://" + redis_host + ":6379");
        redis = redis_owner.get();
        redis->ping();
        redis_available.store(true, memory_order_relaxed);
    } catch (const exception& ex) {
        redis = nullptr;
        redis_available.store(false, memory_order_relaxed);
        spdlog::warn("Redis unavailable at startup: {}. Service will run in degraded mode.", ex.what());
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    //crow::SimpleApp app;
    crow::App<LoggingMiddleware, LifecycleMiddleware, ErrorHandlerMiddleware, AuthMiddleware> app;
    app.signal_clear();

    const int port = stoi(get_env("SERVER_PORT", "8080"));
    const int shutdown_drain_ms = max(0, stoi(get_env("SHUTDOWN_DRAIN_MS", "250")));
    atomic<bool> stop_watcher{false};
    thread signal_watcher([&app, &stop_watcher, shutdown_drain_ms]() {
        while (!stop_watcher.load(memory_order_relaxed)) {
            if (shutdown_signal_received.load(memory_order_relaxed)) {
                if (!shutting_down.exchange(true, memory_order_relaxed)) {
                    spdlog::info("Shutdown signal received. Entering drain mode.");
                    this_thread::sleep_for(chrono::milliseconds(shutdown_drain_ms));
                    app.stop();
                }
                break;
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    });

    CROW_ROUTE(app, "/healthcheck").methods("GET"_method)([]() {
        crow::json::wvalue res;
        res["status"] = "ok";
        return crow::response(200, res);
    });
    CROW_ROUTE(app, "/readiness").methods("GET"_method)([]() {
        crow::json::wvalue res;
        res["status"] = readiness_status();
        res["ready"] = is_ready();
        res["db_ready"] = db_ready.load(memory_order_relaxed);
        res["redis_available"] = redis_available.load(memory_order_relaxed);

        const int code = is_ready() ? 200 : 503;
        return crow::response(code, res);
    });
    CROW_ROUTE(app, "/order/create").methods("POST"_method)(create_order);
    CROW_ROUTE(app, "/order/get/<string>").methods("GET"_method)(get_order);
    CROW_ROUTE(app, "/order/pay").methods("POST"_method)(pay_order);
    CROW_ROUTE(app, "/order/list").methods("GET"_method)(list_orders);
    CROW_ROUTE(app, "/order/delete/<string>").methods("DELETE"_method)(delete_order);
    CROW_ROUTE(app, "/metrics").methods("GET"_method)([] {
        ostringstream os;
        os << "# TYPE total_requests counter\n";
        os << "total_requests " << total_requests.load() << "\n";
        os << "orders_created " << orders_created.load() << "\n";
        os << "orders_paid " << orders_paid.load() << "\n";
        os << "cache_hits " << cache_hits.load() << "\n";
        os << "cache_misses " << cache_misses.load() << "\n";
        os << "overload_rejections " << overload_rejections.load() << "\n";
        os << "shutdown_rejections " << shutdown_rejections.load() << "\n";
        os << "redis_errors " << redis_errors.load() << "\n";
        os << "sqlite_errors " << sqlite_errors.load() << "\n";
        os << "in_flight_requests " << in_flight_requests.load() << "\n";
        os << "redis_available " << (redis_available.load() ? 1 : 0) << "\n";
        os << "service_ready " << (is_ready() ? 1 : 0) << "\n";

        const auto cache_total = cache_hits.load() + cache_misses.load();
        const double cache_ratio = cache_total == 0
            ? 0.0
            : static_cast<double>(cache_hits.load()) / static_cast<double>(cache_total);
        os << "cache_hit_ratio " << cache_ratio << "\n";

        const auto latency_samples = request_duration_samples.load();
        const auto latency_total = request_duration_ms_total.load();
        const double latency_avg = latency_samples == 0
            ? 0.0
            : static_cast<double>(latency_total) / static_cast<double>(latency_samples);
        os << "http_request_duration_ms_total " << latency_total << "\n";
        os << "http_request_duration_ms_count " << latency_samples << "\n";
        os << "http_request_duration_ms_avg " << latency_avg << "\n";
        os << "http_request_duration_ms_max " << request_duration_ms_max.load() << "\n";

        crow::response res;
        res.code = 200;
        res.set_header("Content-Type", "text/plain");
        res.write(os.str());
        return res;
    });


    app.port(port).multithreaded().run();

    stop_watcher.store(true, memory_order_relaxed);
    if (signal_watcher.joinable()) {
        signal_watcher.join();
    }

    if (db != nullptr) {
        sqlite3_close(db);
    }
}
