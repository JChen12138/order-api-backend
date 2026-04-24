#pragma once
#include "crow_all.h"
#include "runtime_config.h"
#include "service_state.h"

struct AuthMiddleware {
    struct context {};

    void before_handle(crow::request& req, crow::response& res, context&) {
        if (service_state::is_probe_path(req.url)) {
            return;
        }

        const std::string api_key_header = req.get_header_value("Authorization");
        if (api_key_header != runtime_config::api_key) {
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
