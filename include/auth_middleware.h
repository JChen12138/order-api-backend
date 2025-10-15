#pragma once
#include "crow_all.h"

struct AuthMiddleware {
    struct context {};

    void before_handle(crow::request& req, crow::response& res, context&) {
        // Allow unauthenticated access to /metrics and /healthcheck
        if (req.url == "/metrics" || req.url == "/healthcheck") {
            return;
        }

        const std::string api_key_header = req.get_header_value("Authorization");

        // Replace with your secret key
        const std::string secret_key = "1234567";

        if (api_key_header != secret_key) {
            res.code = 401;
            res.set_header("Content-Type", "application/json");
            res.write(R"({"error": "Unauthorized"})");
            res.end();
        }
    }

    void after_handle(crow::request& req, crow::response& res, context&) {
        // no-op
    }
};
