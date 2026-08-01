# http-server

[![C++ CI](https://github.com/HenrikGharagyozyan/http-server/actions/workflows/ci.yml/badge.svg)](https://github.com/HenrikGharagyozyan/http-server/actions/workflows/ci.yml)

An HTTP/1.1 server built from scratch in **C++20** on raw POSIX sockets — no networking frameworks. Written to explore how real web servers work under the hood: connection lifecycle, incremental request parsing, backpressure, and per-request arena allocation.

```
curl http://localhost:8080/api/users
```

## Features

- **HTTP/1.1 with keep-alive** — persistent connections with a per-connection buffer; pipelined requests are framed correctly and never lost between reads
- **Incremental request parsing** — requests are read from the TCP stream until complete (`\r\n\r\n` + `Content-Length`), so split packets and slow clients are handled correctly
- **Hardened against malformed input**
  - header / body / URI size limits → `431`, `413`, `414`
  - duplicate or malformed `Content-Length` rejected (`400`)
  - `Transfer-Encoding: chunked` detected and answered with `501` instead of being misparsed
- **Case-insensitive header map** (transparent FNV-1a hash, zero-copy `string_view` lookups)
- **Routing** with method dispatch, `405 Method Not Allowed` + `Allow` header, and a pluggable default handler
- **Static file serving** from an in-memory cache built at startup (MIME detection, `/` → `/index.html` alias, directory-traversal protection)
- **JSON API example** (`GET`/`POST /api/users`) using [nlohmann/json](https://github.com/nlohmann/json)
- **Thread pool with backpressure** — bounded task queue; when the server is saturated, new connections get a clean `503` instead of piling up unbounded
- **Per-request arena allocation** — request/response objects use `std::pmr` containers backed by a [MemCore](https://github.com/HenrikGharagyozyan/MemCore) arena that is reset after every request
- **Robust error handling** — `SIGPIPE`-safe sends, handler exceptions isolated to `500` responses, fd-exhaustion throttling, `std::system_error` with real `errno` context
- **Graceful shutdown** on `SIGINT`/`SIGTERM` (async-signal-safe handler, in-flight requests drained)
- **Structured logging** with [spdlog](https://github.com/gabime/spdlog), including client IP per request

## Architecture

```
                    ┌────────────────────────────────────────────┐
                    │                HttpServer                  │
 accept loop ──────▶│  TcpServer (listen/accept, RAII Socket)    │
                    └──────────────────┬─────────────────────────┘
                                       │ enqueue (bounded queue, 503 on overload)
                                       ▼
                    ┌────────────────────────────────────────────┐
                    │           ThreadPool worker                │
                    │  per-connection loop:                      │
                    │   read → frame → parse → route → serialize │
                    │   (one MemCore arena per request)          │
                    └──────────────────┬─────────────────────────┘
                                       ▼
                    ┌──────────────┐        ┌───────────────────┐
                    │    Router    │───────▶│     Handlers      │
                    │ method + URI │        │ StaticHandler,    │
                    │ 404/405      │        │ user API, custom  │
                    └──────────────┘        └───────────────────┘
```

- `src/server/` — sockets, TCP accept loop, connection handling, thread pool, router, config
- `src/http/` — protocol layer: request/response types, parser, case-insensitive `HeaderMap`, status codes
- `src/handlers/` — application handlers (static files, JSON API)
- `src/utils/` — logging, filesystem helpers

## Building

Requires CMake ≥ 3.20 and a C++20 compiler (GCC or Clang). Dependencies (spdlog, nlohmann/json, GoogleTest, MemCore) are fetched automatically via `FetchContent`.

```bash
git clone https://github.com/HenrikGharagyozyan/http-server.git
cd http-server

# Development build (Debug + ASan/UBSan + -Werror)
cmake --preset dev
cmake --build build -j

# Optimized build
cmake --preset release
cmake --build build_release -j
```

## Running

```bash
# from the repo root (uses ./config.json and serves ./public)
./build_release/http-server

# or with an explicit config file
./build_release/http-server path/to/config.json
```

Try it:

```bash
curl -i http://localhost:8080/                      # static index.html
curl -i http://localhost:8080/api/users             # JSON API
curl -i -X POST http://localhost:8080/api/users \
     -d '{"name": "Ada", "role": "engineer"}'
curl -i -X DELETE http://localhost:8080/api/users   # 405 + Allow: GET, POST
```

### Configuration

`config.json` (all keys optional — defaults shown):

```json
{
    "server": {
        "port": 8080,
        "threads": 4,
        "public_dir": "./public"
    }
}
```

The `PORT` environment variable overrides the configured port (useful in containers).

### Registering routes

```cpp
server::HttpServer app;

app.get("/api/users", handlers::get_users);
app.post("/api/users", handlers::create_user);

handlers::StaticHandler static_files("./public");
app.set_default_handler([&](const http::Request& req) {
    return static_files.handle(req);
});

app.listen(8080, /*threads=*/4);
```

## Testing

91 tests across the parser, router, serialization, handlers, thread pool, and real-socket integration scenarios (split reads, pipelining, oversized requests, concurrent clients).

```bash
cmake --preset dev && cmake --build build -j
ctest --test-dir build --output-on-failure
```

CI runs the full suite on every push with a GCC/Clang × Debug(ASan)/Release matrix.

## Limitations & roadmap

This is an educational project and intentionally not feature-complete. Current model is blocking I/O with a worker pool, which caps concurrency at the thread count. Known gaps, roughly in planned order:

- [ ] `HEAD` support, HTTP/1.0 keep-alive semantics, HTTP version validation (`505`)
- [ ] Percent-decoding of URIs; `Date`/`Server` response headers
- [ ] Fuzzing harness for the parser (libFuzzer)
- [ ] Chunked transfer-encoding decoding (currently rejected with `501`)
- [ ] **epoll-based event loop** — non-blocking I/O to scale past thread-per-connection
- [ ] TLS, gzip, ETag/`304` for the static cache

## Dependencies

| Library | Purpose |
|---|---|
| [spdlog](https://github.com/gabime/spdlog) | logging |
| [nlohmann/json](https://github.com/nlohmann/json) | config + API JSON |
| [MemCore](https://github.com/HenrikGharagyozyan/MemCore) | arena allocator behind `std::pmr` |
| [GoogleTest](https://github.com/google/googletest) | tests |
