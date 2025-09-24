## 🧑‍💻 About This Project

This project was built as part of my backend engineering preparation for real-world job applications. It emphasizes clean API design, observability, caching, and containerization — all implemented in C++ using modern tools.

# Order API Backend (C++, Crow, Redis, SQLite, Docker)

A lightweight RESTful backend service built with C++ and the [Crow](https://github.com/CrowCpp/crow) microframework. This API allows clients to create, pay for, and retrieve orders, with data persisted in SQLite and cached in Redis. Fully containerized using Docker and Docker Compose.

## 🚀 Features

- REST API for order creation, payment, and listing
- SQLite for persistent storage
- Redis for caching order data (TTL: 5 minutes)
- Structured logging via spdlog (`logs/server.log`)
- Fully containerized with Docker and Docker Compose

## 🛠 Tech Stack

- C++17
- Crow (microframework)
- SQLite3
- Redis + redis-plus-plus
- spdlog
- Docker & Docker Compose

## 🏗 Architecture Overview

The service follows a modular, layered architecture:

- **Crow App** handles HTTP routing and middleware
- **SQLite3** stores persistent order records (on-disk)
- **Redis** caches recent order data to reduce DB reads
- **spdlog** provides structured logs for observability
- **Docker Compose** manages Redis and server container orchestration

## 🚀 Getting Started

### Prerequisites

- C++17 compatible compiler (e.g., g++ 11+, clang++)
- CMake 3.15+
- Redis (optional, but recommended for caching)
- SQLite (included via header or linked as a library)

### 🔧 Build Instructions

```bash
# Create a build directory and build the project
mkdir build && cd build
cmake ..
make
./server
```

## 📦 API Endpoints

### Create Order

**POST** `/order/create`

```json
Request Body:
{
  "amount": 99.99
}
```

```json
Response:
{
  "order_no": "ORD17150740421042",
  "amount": 99.99,
  "status": "PENDING",
  "created_at": "2025-05-07 08:27:22"
}
```

## 🧪 Testing the API

Use [Postman](https://www.postman.com/) or `curl` to test endpoints.

### Example: Create Order

```bash
curl -X POST http://localhost:8080/order/create \
  -H "Content-Type: application/json" \
  -d '{"amount": 49.99}'


### Get Order

**GET** `/order/get/{order_no}`

Returns order details. Checks Redis first, falls back to SQLite.

---

### Pay Order

**POST** `/order/pay`

```json
Request Body:
{
  "order_no": "ORD17150740421042"
}
```

Marks order as paid and deletes it from Redis cache.

---

### List Orders

**GET** `/order/list`

Optional query param: `?status=PAID` or `?status=PENDING`

```json
Response:
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

## 🧪 Run It Locally

### Prerequisites

- Docker
- Docker Compose

### Build and Run

```bash
# Build and run using Docker Compose
docker-compose up --build
```

The API will be accessible at:  
👉 `http://localhost:8080`

Test with Postman or curl.

## ✅ Run Unit Tests

This project uses [doctest](https://github.com/doctest/doctest) for lightweight C++ unit testing.

To compile and run tests:

```bash
g++ -std=c++17 -Iinclude -I. test/test_main.cpp test/test_helpers.cpp -o test -lws2_32
./test
```

## 📁 Folder Structure

```txt
.
├── src/              # C++ source files
├── include/          # Header files
├── logs/             # Log output (ignored in .gitignore)
├── CMakeLists.txt    # CMake build file
├── Dockerfile
├── docker-compose.yml
├── README.md
└── ...

---

## 🗂 Logs

Application logs are written to `logs/server.log` inside the container. To view them:

```bash
docker exec -it crow_app cat logs/server.log
```

---

## 📌 Notes

- Redis is used with a 5-minute TTL for fast order reads.
- SQLite is used for persistence and keeps all orders.

---

## ✅ Recent Updates

- Added `/healthcheck` route for uptime monitoring
- Introduced structured logging middleware (writes to `logs/server.log`)
- Added `.gitignore` for logs, database files, and binaries:
gitignore
logs/
*.log
*.db
server.exe

---



