#pragma once
#include "crow_all.h"
#include <string>

crow::response create_order(const crow::request& req);
crow::response get_order(const std::string& order_no);
crow::response pay_order(const crow::request& req);
crow::response list_orders(const crow::request& req);
crow::response delete_order(const std::string& order_no);
