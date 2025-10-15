#include "crow_all.h"
#include <ctime>
#include <string>
#include <iostream>
#include <sqlite3.h>
#include <sw/redis++/redis++.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <cstdlib>
#include "order_routes.h"

using namespace sw::redis;
using namespace std;

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
        exit(1);
    }
}

int main(){
    srand(time(nullptr));

    auto logger = spdlog::basic_logger_mt("file_logger", "logs/server.log");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info); // Or debug
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");

    init_db();
    
    //Make Redis Host Configurable
    string redis_host = get_env("REDIS_HOST", "127.0.0.1");
    Redis real_redis("tcp://" + redis_host + ":6379");
    redis = &real_redis;

    //crow::SimpleApp app;
    crow::App<LoggingMiddleware, ErrorHandlerMiddleware> app;
    CROW_ROUTE(app, "/healthcheck").methods("GET"_method)([]() {
        return "OK";
    });
    CROW_ROUTE(app, "/order/create").methods("POST"_method)(create_order);
    CROW_ROUTE(app, "/order/get/<string>").methods("GET"_method)(get_order);
    CROW_ROUTE(app, "/order/pay").methods("POST"_method)(pay_order);
    CROW_ROUTE(app, "/order/list").methods("GET"_method)(list_orders);
    CROW_ROUTE(app, "/order/delete/<string>").methods("DELETE"_method)(delete_order);


    
    app.port(8080).multithreaded().run();
}