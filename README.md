# Order Service Backend (C++17)

A hands-on C++17 backend/systems project focused on building a multithreaded HTTP service with middleware-based request handling, Redis-backed caching, SQLite persistence, Prometheus-compatible metrics, Dockerized deployment, and end-to-end API testing.

## Project Goal

Build a portfolio-quality backend project that demonstrates:

- Service construction in modern C++
- Request lifecycle control through middleware
- Cache and storage coordination
- Observability and local operability

## Recent Additions (as of April 2026)

- Added readiness vs liveness endpoints so the service can distinguish probe semantics during normal operation and shutdown
- Added shutdown drain mode via signal handling so the service flips out of readiness before stopping
- Added in-flight request limiting with overload rejection behavior
- Upgraded metrics to include overload, shutdown, Redis, SQLite, and request-latency counters
- Changed Redis failure handling to degrade to DB-backed behavior where possible instead of failing every request with `500`

## Current Scope

- Crow-based multithreaded HTTP server
- Middleware for auth, logging, overload protection, and error response normalization
- Order lifecycle endpoints for create, get, pay, list, and delete
- Redis cache-aside read path with TTL-based caching
- SQLite-backed persistence layer
- Readiness/liveness separation with drain-mode shutdown behavior
- Intentional degraded-mode behavior when Redis is unavailable
- Prometheus-compatible counters exposed on `/metrics`
- Docker Compose setup for app, Redis, Prometheus, and test container
- doctest-based unit and API integration coverage

## Features

- Multithreaded HTTP serving with Crow
- API key authentication middleware
- In-flight request limiting with `503` overload shedding
- Request logging with latency measurement
- Centralized JSON error shaping
- Redis caching for order lookups
- SQLite storage for durable order records
- Health check endpoint for liveness probing
- Readiness endpoint that reports ready, degraded, or shutting-down state
- Prometheus-scrapable service counters
- Structured file logging via `spdlog`
- Dockerized local development workflow

## Important Behavior Notes

- The service runs with `app.multithreaded().run()`, so request handling is not limited to a single thread.
- Authentication is implemented as middleware and currently exempts `/metrics`, `/healthcheck`, and `/readiness`.
- The service handles `SIGINT` / `SIGTERM` by entering drain mode first, failing readiness, and then stopping the server.
- New business requests are rejected during shutdown with `503 Service Unavailable` instead of being accepted while the process is exiting.
- In-flight request limiting is enforced in middleware; overload is surfaced as `503 Service Unavailable`.
- The read path follows a cache-aside model: Redis is checked first, and SQLite is used on cache miss.
- State-changing operations invalidate cached order entries instead of trying to update cache and DB in a distributed transaction.
- Redis is treated as an optional acceleration layer for most request paths; if Redis is unavailable, reads fall back to SQLite and write-side cache population/invalidation becomes best effort.
- `/metrics` currently exposes richer service-level counters and latency aggregates, but not full labeled per-route histograms.
- The API key is hard-coded in the current implementation and should be treated as demo-only, not production-ready secret handling.

## Project Structure

```text
.
|-- include/
|   |-- auth_middleware.h
|   |-- helpers.hpp
|   |-- metrics.h
|   |-- order_routes.h
|   |-- service_state.h
|   `-- crow_all.h
|-- src/
|   |-- main.cpp
|   `-- order_routes.cpp
|-- test/
|   |-- test_endpoints.cpp
|   |-- test_helpers.cpp
|   `-- test_main.cpp
|-- logs/
|-- Dockerfile
|-- docker-compose.yml
|-- prometheus.yml
|-- CMakeLists.txt
`-- README.md
```

## Build and Run

### Requirements

- C++17 compiler
- CMake 3.12+
- SQLite3
- Redis
- Docker and Docker Compose for the containerized workflow

### Canonical Local Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Run:

```bash
./bin/server
```

Service endpoint:

```text
http://localhost:8080
```

### Docker Demo

A minimal Docker stack is included for the app, Redis, Prometheus, and the test container.

Run:

```bash
docker-compose up --build
```

Endpoints:

```text
App:         http://localhost:8080
Liveness:    http://localhost:8080/healthcheck
Readiness:   http://localhost:8080/readiness
Metrics:     http://localhost:8080/metrics
Prometheus:  http://localhost:9090
```

Notes:

- `REDIS_HOST=redis` is injected through Docker Compose so the app uses the Redis container by service name.
- Logs are mounted to `./logs`.
- The SQLite database file is mounted through Compose for persistence across container restarts.
- `MAX_INFLIGHT_REQUESTS`, `SERVER_PORT`, and `SHUTDOWN_DRAIN_MS` can be configured with environment variables.

### CMake Status

The repository includes a working CMake path for the server and test targets on the current local setup. The current CMake configuration is environment-specific on Windows because it links directly against local `vcpkg` library paths.

## Tests

### Docker Integration Test Path

```bash
docker-compose run test
```

Covered areas:

- Order creation success path
- Invalid order amount handling
- Order payment flow
- Invalid or missing `order_no` handling
- Delete flow across API, cache, and persistent storage
- Readiness probe response shape
- Metrics exposure for lifecycle and overload counters

Verified in repo:

- doctest-based endpoint coverage exists in `test/test_endpoints.cpp`
- helper validation coverage exists in `test/test_helpers.cpp`

## Metrics Overview

| Metric | Type | Description |
|--------|------|-------------|
| `total_requests` | Counter | Total handled requests across endpoints |
| `orders_created` | Counter | Orders created successfully |
| `orders_paid` | Counter | Orders marked as paid |
| `cache_hits` | Counter | Redis hits on order lookup |
| `cache_misses` | Counter | Redis misses on order lookup |
| `overload_rejections` | Counter | Requests rejected because the in-flight limit was exceeded |
| `shutdown_rejections` | Counter | Requests rejected while the service was draining for shutdown |
| `redis_errors` | Counter | Redis operation failures and unavailable-client events |
| `sqlite_errors` | Counter | SQLite prepare/step failures |
| `in_flight_requests` | Gauge-style counter | Current business requests being processed |
| `http_request_duration_ms_*` | Aggregate counters | Total, count, average, and max request duration in milliseconds |
| `cache_hit_ratio` | Derived gauge | Cache hit ratio computed from hits and misses |

## Architecture (Request -> Middleware -> Cache/DB)

```text
Layer 1: Request Entry
  Crow HTTP server
  - accepts client requests
  - dispatches routes on a multithreaded server

Layer 2: Middleware
  AuthMiddleware
  - validates Authorization header
  - skips /metrics, /healthcheck, and /readiness

  LifecycleMiddleware
  - rejects new business traffic during shutdown drain
  - applies in-flight overload protection
  - tracks current request concurrency

  LoggingMiddleware
  - records request path, status, and latency

  ErrorHandlerMiddleware
  - normalizes error responses
  - logs failed requests

Layer 3: Handlers + Dependencies
  Order handlers
  - use Redis for cache lookup / invalidation
  - use SQLite for persistent storage
  - update in-process metrics counters
```

## Execution Flow Overview

This project follows a simple backend service flow: Request -> Middleware -> Handler -> Dependency.

1. Request Entry
- Crow accepts an HTTP request and routes it to the matching handler
- the app runs in multithreaded mode, so multiple requests can be served concurrently

2. Middleware Processing
- `AuthMiddleware` validates the API key for protected routes
- `LifecycleMiddleware` blocks new work during shutdown and sheds load when the request budget is exceeded
- `LoggingMiddleware` records start time before the handler and logs latency after completion
- `ErrorHandlerMiddleware` converts empty error bodies into normalized JSON responses

3. Handler Logic
- create writes a new order to SQLite and then best-effort populates Redis with a TTL-based cache entry
- get checks Redis first and falls back to SQLite on cache miss or Redis failure
- pay updates SQLite state and then best-effort invalidates the cached order
- delete removes the order from SQLite and then best-effort invalidates Redis
- list reads order state from SQLite directly

4. Observability Surface
- service-level counters are exposed through `/metrics`
- liveness is exposed through `/healthcheck`
- readiness is exposed through `/readiness`
- request logs are written to `logs/server.log`

## Engineering Positioning

This project is intentionally different from a business CRUD demo. The value is less about domain complexity and more about showing backend/platform fundamentals in C++:

- multithreaded request serving
- middleware-based separation of concerns
- cache/database interaction
- service observability
- containerized local reproducibility
- automated behavioral verification

For interviews, I would describe it as a small service-platform exercise rather than as an order-management product.

## Roadmap

- Move API key handling to environment-based configuration
- Add per-route latency histograms and richer error metrics
- Improve delete correctness by checking affected rows explicitly
- Add configuration support for port, TTL, and DB path
- Add load testing to show throughput and concurrency behavior
- Explore replacing SQLite with a client/server database for stronger infra discussion

## Author

**Weijia (J) Chen**  
C++ Backend / Systems Developer

## License

MIT License (c) 2025 Weijia Chen
