# Loom HTTP Server Library

A modular, high-performance HTTP server library for the Loom programming language.

## Features

- **Request Parsing**: Fast parsing of HTTP request methods (`GET`, `POST`, `PUT`, `DELETE`), URL paths, query parameters (`?key=val`), multi-line headers, and body payloads.
- **Response Builder**: Fluent builder pattern for HTTP status codes, custom headers, HTML, JSON, and plain text responses.
- **Connection Management**: Clean TCP listener setup with `SO_REUSEADDR`, client socket acceptance, timeout configuration, and response dispatching.

---

## Response Writers

| Function | Signature | Description |
|---|---|---|
| `Res_New` | `() -> Response_Writer` | Creates a new response writer (200 OK) |
| `Res_Status` | `(Res, Code: Signed_32) -> Response_Writer` | Sets the HTTP status code (e.g. 200, 201, 404, 500) |
| `Res_Header` | `(Res, Name: String, Val: String) -> Response_Writer` | Adds an HTTP response header |
| `Res_Header_Text` | `(Res, Name: String, Val: Text) -> Response_Writer` | Adds an HTTP response header from dynamic Text |
| `Res_Html` | `(Res, Html_Str: String) -> Response_Writer` | Sets `Content-Type: text/html; charset=utf-8` and appends HTML |
| `Res_Html_Text` | `(Res, Html_Str: Text) -> Response_Writer` | Sets `Content-Type: text/html; charset=utf-8` and appends HTML Text |
| `Res_Json` | `(Res, Json_Str: String) -> Response_Writer` | Sets `Content-Type: application/json` and appends JSON |
| `Res_Json_Text` | `(Res, Json_Str: Text) -> Response_Writer` | Sets `Content-Type: application/json` and appends JSON Text |
| `Res_Text` | `(Res, Content: String) -> Response_Writer` | Sets `Content-Type: text/plain; charset=utf-8` and appends text |
| `Res_Text_Text` | `(Res, Content: Text) -> Response_Writer` | Sets `Content-Type: text/plain; charset=utf-8` and appends text |
| `Res_Send` | `(Res, Client_Fd: Signed_32) -> Signed_32` | Formats status line, `Content-Length`, headers, and sends |

---

## Web Server Proof of Concept (PoC)

An interactive multi-route web server example is provided in [examples/web_server_poc.loom](file:///Users/lokesh/Desktop/loom-libraries/libraries/http-server/examples/web_server_poc.loom).

### Endpoints

| Method | Endpoint | Response Format | Purpose |
|---|---|---|---|
| `GET` | `/` | `text/html` | Modern interactive web UI explaining server architecture |
| `GET` | `/api/status` | `application/json` | Returns server health, version, and enabled protocol stack |
| `POST` | `/api/echo` | `application/json` | Echoes the incoming HTTP request payload |
| `GET` | `/healthz` | `text/plain` | Kubernetes-style liveness probe (`OK`) |
| `GET` | `/api/shutdown` | `application/json` | Graceful server termination trigger |
| `*` | Any other path | `application/json` | 404 Not Found error response |

### Compiling and Running the PoC Server

```bash
# 1. Compile with loomc and link with clang
loomc -I stdlib -I /Users/lokesh/Desktop/loom-libraries/libraries/http-server/src \
      --object examples/web_server_poc.loom poc.o --entry
clang poc.o libloom_rt.a -o web_server_poc

# 2. Run the server
./web_server_poc
```

### Testing with `curl`

```bash
# HTML Web Interface
curl -i http://127.0.0.1:8088/

# JSON Metadata
curl -i http://127.0.0.1:8088/api/status

# Health Probe
curl -i http://127.0.0.1:8088/healthz

# JSON Echo
curl -i -X POST http://127.0.0.1:8088/api/echo \
     -H "Content-Type: application/json" \
     -d '{"message":"Hello Loom!"}'

# 404 Handler
curl -i http://127.0.0.1:8088/unknown

# Graceful Shutdown
curl -i http://127.0.0.1:8088/api/shutdown
```

### Automated Testing

Run the automated test suite in `tests/test_web_server_poc.py`:

```bash
python3 tests/test_web_server_poc.py
```
