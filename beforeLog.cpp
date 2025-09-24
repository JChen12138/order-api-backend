#include "crow_all.h"
#include <ctime>
#include <string>
#include <iostream>
#include <sqlite3.h>
#include <sw/redis++/redis++.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
using namespace sw::redis;
using namespace std;

sqlite3* db = nullptr;
Redis* redis = nullptr;

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
        cerr << "Can't open DB: " << sqlite3_errmsg(db) << endl;
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
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
        exit(1);
    }
}

int main(){
    srand(time(nullptr));

    init_db();

    Redis real_redis("tcp://127.0.0.1:6379");
    redis = &real_redis;

    crow::SimpleApp app;
    CROW_ROUTE(app, "/order/create").methods("POST"_method)([](const crow::request& req){
        auto body = crow::json::load(req.body);
        if(!body || !body.has("amount")){
            return crow::response(400, "Missing amount");
        }
        double amount = body["amount"].d();
        string order_no = generate_order_no();
        time_t now = time(nullptr);

        sqlite3_stmt* stmt;
        const char* sql =  "INSERT INTO orders (order_no, amount, status, created_at, paid_at) VALUES (?, ?, ?, ?, ?);";
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, order_no.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, amount);
        sqlite3_bind_text(stmt, 3, "PENDING", -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, now);
        sqlite3_bind_int64(stmt, 5, 0);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["order_no"] = order_no;
        res["amount"] = amount;
        res["status"] = "PENDING";
        res["created_at"] = format_time(now);

        ostringstream oss;
        oss << res.dump();
        try {
            redis->set("order:" + order_no, oss.str(), chrono::seconds(300)); 
            cout << "[INFO] Cached order " << order_no << " in Redis (TTL: 300s)" << endl;
        } catch (const sw::redis::Error& err) {
            cerr << "[ERROR] Redis SET failed: " << err.what() << endl;
        }
        return crow::response(res);
    });

    CROW_ROUTE(app, "/order/get/<string>").methods("GET"_method)([](const string& order_no){
        try {
            auto val = redis->get("order:" + order_no);
            if (val) {
                cout << "[INFO] Redis cache hit for order " << order_no << endl;
                return crow::response(*val);
            } else {
                cout << "[INFO] Redis cache miss for order " << order_no << endl;
            }
        } catch (const sw::redis::Error& err) {
            cerr << "[ERROR] Redis GET failed: " << err.what() << endl;
        }

        sqlite3_stmt* stmt;
        const char* sql = "SELECT amount, status, created_at, paid_at FROM orders WHERE order_no = ?;";
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, order_no.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return crow::response(404, "Order not found");
        }

        double amount = sqlite3_column_double(stmt, 0);
        string status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        time_t created_at = sqlite3_column_int64(stmt, 2);
        time_t paid_at = sqlite3_column_int64(stmt, 3);
        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["order_no"] = order_no;
        res["amount"] = amount;
        res["status"] = status;
        res["created_at"] = format_time(created_at);
        res["paid_at"] = paid_at == 0 ? crow::json::wvalue() : format_time(paid_at);
        return crow::response(res);
    });

    CROW_ROUTE(app, "/order/pay").methods("POST"_method)([](const crow::request& req){
        auto body = crow::json::load(req.body);
        if(!body || !body.has("order_no")){
            return crow::response(400, "Missing order_no");
        }
        string order_no = body["order_no"].s();
        sqlite3_stmt* stmt;
        const char* select_sql = "SELECT amount, status, created_at FROM orders WHERE order_no = ?;";
        sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, order_no.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return crow::response(404, "Order not found");
        }

        double amount = sqlite3_column_double(stmt, 0);
        string status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        time_t created_at = sqlite3_column_int64(stmt, 2);
        sqlite3_finalize(stmt);

        if (status == "PAID") {
            return crow::response(400, "Already paid");
        }

        try {
            redis->del("order:" + order_no);
            cout << "[INFO] Deleted order " << order_no << " from Redis cache" << endl;
        } catch (const sw::redis::Error& err) {
            cerr << "[ERROR] Redis DEL failed: " << err.what() << endl;
        }
        

        time_t now = time(nullptr);
        const char* update_sql = "UPDATE orders SET status = 'PAID', paid_at = ? WHERE order_no = ?;";
        sqlite3_prepare_v2(db, update_sql, -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, now);
        sqlite3_bind_text(stmt, 2, order_no.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["order_no"] = order_no;
        res["status"] = "PAID";
        res["amount"] = amount;
        res["created_at"] = format_time(created_at);
        res["paid_at"] = format_time(now);
        return crow::response(res);

    });

    CROW_ROUTE(app, "/order/list").methods("GET"_method)([](const crow::request req){
        string query = req.url_params.get("status") ? req.url_params.get("status") : "";
        const char* sql_all = "SELECT order_no, amount, status, created_at, paid_at FROM orders;";
        const char* sql_filtered = "SELECT order_no, amount, status, created_at, paid_at FROM orders WHERE status = ?;";
        sqlite3_stmt* stmt;
        if(query.empty()){
            sqlite3_prepare_v2(db, sql_all, -1, &stmt, nullptr);
        }
        else{
            sqlite3_prepare_v2(db, sql_filtered, -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_STATIC);
        }
        crow::json::wvalue result;
        auto& arr = result["orders"];
        int index = 0;
        while(sqlite3_step(stmt) == SQLITE_ROW){
            crow::json::wvalue order;
            order["order_no"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            order["amount"] = sqlite3_column_double(stmt, 1);
            order["status"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            order["created_at"] = format_time(sqlite3_column_int64(stmt, 3));
            order["paid_at"] = sqlite3_column_int64(stmt, 4) == 0 ? crow::json::wvalue() : format_time(sqlite3_column_int64(stmt, 4));
            arr[index++] = move(order);
        }
        sqlite3_finalize(stmt);
        return crow::response(result);

    });
    
    app.port(8080).multithreaded().run();
}