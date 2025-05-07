#include "crow_all.h"
#include <ctime>
#include <cstdlib>
#include <string>
#include <unordered_map>
using namespace std;

struct Order{
    double amount;
    string status;
    time_t created_at;
    time_t paid_at;
};

unordered_map<string, Order> order_store;
mutex store_mutex;

string generate_order_no(){
    srand(time(nullptr));
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

int main(){
    crow::SimpleApp app;
    CROW_ROUTE(app, "/order/create").methods("POST"_method)([](const crow::request& req){
        auto body = crow::json::load(req.body);
        if(!body || !body.has("amount")){
            return crow::response(400, "Missing amount");
        }
        double amount = body["amount"].d();
        string order_no = generate_order_no();
        time_t now = time(nullptr);
        Order order{amount, "PENDING", now, 0};
        {
            lock_guard<mutex> lock(store_mutex);
            order_store[order_no] = order;
        }
        crow::json::wvalue res;
        res["order_no"] = order_no;
        res["amount"] = order.amount;
        res["status"] = order.status;
        res["created_at"] = format_time(order.created_at);
        return crow::response(res);
    });

    CROW_ROUTE(app, "/order/get/<string>").methods("GET"_method)([](const string& order_no){
        Order order;
        {
            lock_guard<mutex> lock(store_mutex);
            auto it= order_store.find(order_no);
            if(it == order_store.end()){
                return crow::response(404, "Order not found");
            }
            order = it->second;
        }
        crow::json::wvalue res;
        res["order_no"] = order_no;
        res["amount"] = order.amount;
        res["status"] = order.status;
        res["created_at"] = format_time(order.created_at);
        res["paid_at"] = order.paid_at == 0 ? crow::json::wvalue() : format_time(order.paid_at);
        return crow::response(res);
    });

    CROW_ROUTE(app, "/order/pay").methods("POST"_method)([](const crow::request& req){
        auto body = crow::json::load(req.body);
        if(!body || !body.has("order_no")){
            return crow::response(400, "Missing order_no");
        }
        string order_no = body["order_no"].s();
        Order order;
        {
            lock_guard<mutex> lock(store_mutex);
            auto it = order_store.find(order_no);
            if(it == order_store.end()){
                return crow::response(404, "Order not found");
            }
            order = it->second;
            if(order.status == "PAID"){
                return crow::response(400, "Already Paid");
            }
            it->second.status = "PAID";
            it->second.paid_at = time(nullptr);
        }

        crow::json::wvalue res;
        res["order_no"] = order_no;
        res["status"] = order.status;
        res["amount"] = order.amount;
        res["paid_at"] = format_time(order.paid_at);
        return crow::response(res);

    });

    app.port(8080).multithreaded().run();
}