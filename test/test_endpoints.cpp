#include "doctest.h"
#include "crow_all.h"
#include <httplib.h>  // lightweight HTTP client lib for testing
#include <string>
using namespace std;

#ifndef TEST_API_HOST
#define TEST_API_HOST "localhost"
#endif

// API key for authentication middleware
const char* API_KEY = "1234567";

// Helper to attach Authorization header to every request
httplib::Headers auth_header = {{"Authorization", API_KEY}};

// ---------------------------------------------------------

TEST_CASE("Delete order removes from Redis and database") {
    httplib::Client cli("http://" TEST_API_HOST ":8080");

    // Step 1: Create an order
    string body = R"({"amount": 99.99})";
    auto res = cli.Post("/order/create", auth_header, body, "application/json");
    CHECK(res != nullptr);
    CHECK(res->status == 200);

    auto json = crow::json::load(res->body);
    string order_no = json["order_no"].s();

    // Step 2: Confirm order exists
    auto res2 = cli.Get(("/order/get/" + order_no).c_str(), auth_header);
    CHECK(res2 != nullptr);
    CHECK(res2->status == 200);

    // Step 3: Delete the order
    auto res3 = cli.Delete(("/order/delete/" + order_no).c_str(), auth_header);
    CHECK(res3 != nullptr);
    CHECK(res3->status == 200);

    // Step 4: Confirm deletion (should return 404)
    auto res4 = cli.Get(("/order/get/" + order_no).c_str(), auth_header);
    CHECK(res4 != nullptr);
    CHECK(res4->status == 404);
}

// ---------------------------------------------------------

TEST_CASE("Creating order with invalid amount returns 400") {
    httplib::Client cli("http://" TEST_API_HOST ":8080");

    string zero_body = R"({"amount": 0})";
    auto res_zero = cli.Post("/order/create", auth_header, zero_body, "application/json");
    CHECK(res_zero != nullptr);
    CHECK(res_zero->status == 400);

    string negative_body = R"({"amount": -1})";
    auto res_negative = cli.Post("/order/create", auth_header, negative_body, "application/json");
    CHECK(res_negative != nullptr);
    CHECK(res_negative->status == 400);
}

// ---------------------------------------------------------

TEST_CASE("Paying for a created order updates state") {
    httplib::Client cli("http://" TEST_API_HOST ":8080");

    // Step 1: Create an order
    string create_body = R"({"amount": 123.45})";
    auto res_create = cli.Post("/order/create", auth_header, create_body, "application/json");
    CHECK(res_create != nullptr);
    CHECK(res_create->status == 200);

    auto json = crow::json::load(res_create->body);
    string order_no = json["order_no"].s();

    // Step 2: Pay for it
    string pay_body = R"({"order_no": ")" + order_no + R"("})";
    auto res_pay = cli.Post("/order/pay", auth_header, pay_body, "application/json");
    CHECK(res_pay != nullptr);
    CHECK(res_pay->status == 200);

    // Step 3: Check status updated
    auto res_get = cli.Get(("/order/get/" + order_no).c_str(), auth_header);
    CHECK(res_get != nullptr);
    CHECK(res_get->status == 200);

    auto json_get = crow::json::load(res_get->body);
    CHECK(json_get["status"].s() == "PAID");
}

// ---------------------------------------------------------

TEST_CASE("Paying with invalid or missing order_no returns 400") {
    httplib::Client cli("http://" TEST_API_HOST ":8080");

    string empty_body = R"({"order_no": ""})";
    auto res_empty = cli.Post("/order/pay", auth_header, empty_body, "application/json");
    CHECK(res_empty != nullptr);
    CHECK(res_empty->status == 400);

    string missing_body = R"({})";
    auto res_missing = cli.Post("/order/pay", auth_header, missing_body, "application/json");
    CHECK(res_missing != nullptr);
    CHECK(res_missing->status == 400);
}
