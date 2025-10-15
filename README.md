## 🧑‍💻 About This Project
![Docker](https://img.shields.io/badge/docker-ready-blue)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)

This project was built as part of my backend engineering preparation for real-world job applications. It emphasizes clean API design, observability, caching, containerization, and testability, all implemented in **modern C++**.

# Order API Backend (C++, Crow, Redis, SQLite, Docker)

A lightweight RESTful backend service built with C++ and the [Crow](https://github.com/CrowCpp/crow) microframework. This API allows clients to create, pay for, delete, and retrieve orders, with data persisted in SQLite and cached in Redis. The project is fully containerized and supports unit + integration testing using Docker.

---

## 🚀 Features

- REST API for:
  - Creating, paying, deleting, and listing orders
- SQLite for persistent storage
- Redis for caching order data (TTL: 5 minutes)
- End-to-end integration tests via [doctest](https://github.com/doctest/doctest)
- Structured logging via spdlog (`logs/server.log`)
- Fully containerized with Docker and Docker Compose
- Modular codebase with routing logic separated by responsibility

---

## 🛠 Tech Stack

- **C++17**
- **Crow** (HTTP microframework)
- **SQLite3**
- **Redis + redis-plus-plus**
- **spdlog** (structured logging)
- **Docker** & **Docker Compose**
- **doctest** (unit + integration testing)

---

## 🏗 Architecture Overview

```
Client <--> Crow HTTP Server <--> SQLite3 (DB)
                             |
                             └--> Redis (cache, 5 min TTL)
```

- `Crow` handles routing + middleware
- `Redis` caches recently accessed orders for fast reads
- `SQLite` persistently stores all order records
- `spdlog` logs structured logs to disk
- `Docker Compose` manages Redis, test, and server containers
- `doctest` handles automated test coverage of critical flows

---

## 📁 Folder Structure

```txt
.
├── include/                 # C++ header files (helpers, routes)
├── src/                     # Main and route implementation files
├── test/                    # Unit & integration tests (doctest-based)
├── logs/                   # Log output (ignored in .gitignore)
├── Dockerfile               # Docker app build
├── docker-compose.yml       # Multi-container orchestration
├── CMakeLists.txt           # CMake build script
└── README.md
```

---

## 📦 API Endpoints

### ✅ Create Order

**POST** `/order/create`

```json
{
  "amount": 99.99
}
```

Returns a new order record.

**Response:**
```json
{
  "order_no": "ORD17150740421042",
  "amount": 99.99,
  "status": "PENDING",
  "created_at": "2025-05-07 08:27:22"
}
```

---

### 💸 Pay Order

**POST** `/order/pay`

```json
{
  "order_no": "ORD17150740421042"
}
```

Marks an order as paid and removes it from Redis cache.

---

### 🔍 Get Order

**GET** `/order/get/{order_no}`

Checks Redis first. Falls back to SQLite on cache miss.

---

### 📃 List Orders

**GET** `/order/list`

Optional query param: `?status=PAID` or `?status=PENDING`

**Response:**
```json
{
  "orders": [
    {
      "order_no": "...",
      "amount": ...,
      "status": "PAID",
      "created_at": "...",
      "paid_at": "..."
    }
  ]
}
```

---

### ❌ Delete Order

**DELETE** `/order/delete/{order_no}`

Deletes the order from both SQLite and Redis.

---


## 🚀 Getting Started

### Prerequisites

- C++17-compatible compiler (e.g., `g++ 11+`)
- CMake 3.15+
- Redis server (local or via Docker)
- SQLite3 installed

---

### 🔧 Manual Build (CMake)

```bash
mkdir build && cd build
cmake ..
make
./server
```

---

## 🧪 Testing the API

You can use `curl` or [Postman](https://www.postman.com/) to interact with the endpoints.

### Example: Create Order

```bash
curl -X POST http://localhost:8080/order/create   -H "Content-Type: application/json"   -d '{"amount": 49.99}'
```

---

## 🐳 Run with Docker

### Prerequisites

- Docker
- Docker Compose

---

### Run the Full Stack

```bash
docker-compose up --build
```

Then access the service at:  
👉 `http://localhost:8080`

---

### Docker Compose Setup Highlights

- Redis volume is persisted via `./redis_data`
- Application logs are volume-mapped to `./logs/`
- Source is mounted for rebuild convenience

---

## 🧪 Run All Tests in Docker

Tests include:
- Unit tests (helpers, validators)
- Integration tests (`order/create`, `order/pay`, `order/delete`)

```bash
docker-compose run test
# In separate terminal while docker is running:
curl -X GET http://localhost:8080/order/list
```

Expected output:

```bash
[doctest] test cases: 3 | 3 passed | 0 failed
```

Note: A small `sleep` delay is included in tests to ensure the Redis server is ready before API calls begin.

---

## 🗂 Logs

Logs are saved at `logs/server.log` in your project root (mounted from Docker volume). View them with:

```bash
cat logs/server.log
```

---

## ✅ Run Unit Tests Individually

This project uses [doctest](https://github.com/doctest/doctest) for lightweight C++ unit testing.

To compile and run tests manually (without Docker), use the following commands based on your OS:

```bash
### On Windows (MinGW):
g++ -std=c++17 -Iinclude -I. test/test_main.cpp test/test_helpers.cpp -o test -lws2_32
./test

### On Linux/macOS:
g++ -std=c++17 -Iinclude -I. test/test_main.cpp test/test_helpers.cpp -o test
./test
```

---

## 📝 Minor Updates – Sept 29, 2025

- Added `/healthcheck` route for uptime monitoring
- Introduced structured logging middleware (writes to `logs/server.log`)
- Added `.gitignore` for logs, database files, and binaries:
Contents of `.gitignore`:
logs/
*.log
*.db
server.exe

---

## ✅ Recent Updates (Oct 2025)

- ✅ Added `/order/delete/:id` route
- ✅ Added full end-to-end tests (`test_endpoints.cpp`)
- ✅ Separated route logic into `order_routes.cpp`
- ✅ Added delay in `test_main.cpp` to wait for Redis on startup
- ✅ Fixed `static_assert` with `fmt` + Unicode output
- ✅ Linked proper paths via CMake and Docker `g++`
- ✅ Docker build success (redis++, sqlite3, fmt)
- ✅ Docker volume mapping for logs and Redis data
- ✅ Docker unit tests working (exit code 0)
- ✅ Clean logs: all tests pass, server starts cleanly

---

## 🔧 Notes

- Redis TTL is set to 5 minutes.
- Order status is either `PENDING` or `PAID`.
- Redis is optional — fallback to DB works gracefully.
- SQLite DB and logs are persisted via volumes.

---

## 📄 License

This project is licensed under the [MIT License](./LICENSE).  
Feel free to use, modify, and distribute it for personal or commercial use.



