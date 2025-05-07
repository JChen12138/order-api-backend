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

---

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
docker compose build --no-cache
docker compose up
```

The API will be accessible at:  
👉 `http://localhost:8080`

Test with Postman or curl.

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

## 🧑‍💻 Author

Built as a demonstration project for backend development in C++.

