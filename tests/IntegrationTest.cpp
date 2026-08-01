#include <gtest/gtest.h>
#include "server/HttpServer.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace
{

    // Ask the kernel for a free port by binding to port 0
    uint16_t get_free_port()
    {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        EXPECT_GE(fd, 0);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        EXPECT_EQ(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

        socklen_t len = sizeof(addr);
        EXPECT_EQ(::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len), 0);
        uint16_t port = ntohs(addr.sin_port);
        ::close(fd);
        return port;
    }

    // Minimal blocking HTTP client for driving the server over a real socket
    class TestClient
    {
    public:
        explicit TestClient(uint16_t port)
        {
            fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
            if (fd_ < 0)
                return;

            timeval tv{8, 0};
            ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

            if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
            {
                ::close(fd_);
                fd_ = -1;
            }
        }

        ~TestClient() { close(); }

        TestClient(const TestClient&) = delete;
        TestClient& operator=(const TestClient&) = delete;

        [[nodiscard]] bool connected() const { return fd_ >= 0; }

        void close()
        {
            if (fd_ >= 0)
            {
                ::close(fd_);
                fd_ = -1;
            }
        }

        // Returns false if the peer closed the connection mid-send.
        // Tests that provoke an early rejection ignore the return value.
        bool send_all(std::string_view data)
        {
            size_t sent_total = 0;
            while (sent_total < data.size())
            {
                ssize_t sent = ::send(fd_, data.data() + sent_total,
                                      data.size() - sent_total, MSG_NOSIGNAL);
                if (sent <= 0)
                    return false;
                sent_total += static_cast<size_t>(sent);
            }
            return true;
        }

        bool send_byte_by_byte(std::string_view data)
        {
            for (char c : data)
            {
                if (::send(fd_, &c, 1, MSG_NOSIGNAL) != 1)
                    return false;
                std::this_thread::sleep_for(1ms);
            }
            return true;
        }

        // Reads exactly one HTTP response, framed by its Content-Length header.
        // Returns "" if the connection closes/times out before headers arrive.
        std::string recv_response()
        {
            size_t headers_end;
            while ((headers_end = buf_.find("\r\n\r\n")) == std::string::npos)
            {
                if (!fill())
                    return "";
            }

            size_t content_length = 0;
            const std::string cl_key = "Content-Length: ";
            size_t cl_pos = buf_.find(cl_key);
            if (cl_pos != std::string::npos && cl_pos < headers_end)
            {
                content_length = std::stoul(buf_.substr(cl_pos + cl_key.size()));
            }

            size_t total = headers_end + 4 + content_length;
            while (buf_.size() < total)
            {
                if (!fill())
                    break;
            }

            std::string response = buf_.substr(0, std::min(total, buf_.size()));
            buf_.erase(0, std::min(total, buf_.size()));
            return response;
        }

        // Returns true if the server has closed the connection (EOF)
        bool peer_closed()
        {
            while (fill()) {}
            return eof_;
        }

    private:
        bool fill()
        {
            char tmp[4096];
            ssize_t n = ::recv(fd_, tmp, sizeof(tmp), 0);
            if (n > 0)
            {
                buf_.append(tmp, static_cast<size_t>(n));
                return true;
            }
            // A reset (server closed with unread data pending) counts as closed;
            // only a timeout (EAGAIN/EWOULDBLOCK) means the peer might still be there.
            if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
                eof_ = true;
            return false;
        }

        int fd_{-1};
        std::string buf_;
        bool eof_{false};
    };

    int status_of(const std::string& response)
    {
        // "HTTP/1.1 XXX ..."
        if (response.size() < 12 || !response.starts_with("HTTP/1.1 "))
            return -1;
        return std::stoi(response.substr(9, 3));
    }

    std::string body_of(const std::string& response)
    {
        size_t pos = response.find("\r\n\r\n");
        return (pos == std::string::npos) ? "" : response.substr(pos + 4);
    }

} // namespace


class IntegrationTest : public ::testing::Test
{
protected:
    static std::unique_ptr<server::HttpServer> app;
    static std::thread server_thread;
    static uint16_t port;
    static std::atomic<bool> server_failed;

    static void SetUpTestSuite()
    {
        std::signal(SIGPIPE, SIG_IGN);
        port = get_free_port();

        app = std::make_unique<server::HttpServer>();

        app->get("/hello", [](const http::Request& req)
        {
            std::pmr::memory_resource* mr = req.uri.get_allocator().resource();
            http::Response res(mr);
            res.status_code = http::StatusCode::OK;
            res.body = "hello world";
            res.headers["Content-Type"] = "text/plain";
            res.headers["Content-Length"] = std::pmr::string(std::to_string(res.body.size()));
            return res;
        });

        app->post("/echo", [](const http::Request& req)
        {
            std::pmr::memory_resource* mr = req.uri.get_allocator().resource();
            http::Response res(mr);
            res.status_code = http::StatusCode::OK;
            res.body = req.body;
            res.headers["Content-Type"] = "application/octet-stream";
            res.headers["Content-Length"] = std::pmr::string(std::to_string(res.body.size()));
            return res;
        });

        server_thread = std::thread([]
        {
            try
            {
                server::Config config;
                config.port = port;
                config.threads = 4;
                app->listen(config);
            }
            catch (...)
            {
                server_failed = true;
            }
        });

        // Wait until the server accepts connections
        bool up = false;
        for (int i = 0; i < 250 && !server_failed; ++i)
        {
            TestClient probe(port);
            if (probe.connected())
            {
                up = true;
                break;
            }
            std::this_thread::sleep_for(20ms);
        }
        ASSERT_TRUE(up) << "server did not start listening on port " << port;
    }

    static void TearDownTestSuite()
    {
        app->stop();
        if (server_thread.joinable())
            server_thread.join();
        app.reset();
    }
};

std::unique_ptr<server::HttpServer> IntegrationTest::app;
std::thread IntegrationTest::server_thread;
uint16_t IntegrationTest::port = 0;
std::atomic<bool> IntegrationTest::server_failed{false};


// ==========================================================
// Happy paths
// ==========================================================

TEST_F(IntegrationTest, SimpleGetReturnsHandlerResponse)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("GET /hello HTTP/1.1\r\nHost: t\r\nConnection: close\r\n\r\n");
    std::string response = client.recv_response();

    EXPECT_EQ(status_of(response), 200);
    EXPECT_EQ(body_of(response), "hello world");
    EXPECT_NE(response.find("Connection: close\r\n"), std::string::npos);
}

TEST_F(IntegrationTest, UnknownRouteReturns404)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("GET /no-such-route HTTP/1.1\r\nConnection: close\r\n\r\n");

    EXPECT_EQ(status_of(client.recv_response()), 404);
}

TEST_F(IntegrationTest, PostEchoRoundTrip)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    const std::string body = "The quick brown fox";
    client.send_all("POST /echo HTTP/1.1\r\nContent-Length: " + std::to_string(body.size()) +
                    "\r\nConnection: close\r\n\r\n" + body);
    std::string response = client.recv_response();

    EXPECT_EQ(status_of(response), 200);
    EXPECT_EQ(body_of(response), body);
}

// ==========================================================
// Framing: split sends, pipelining, keep-alive
// ==========================================================

TEST_F(IntegrationTest, ByteByByteSendIsReassembled)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_byte_by_byte("GET /hello HTTP/1.1\r\nConnection: close\r\n\r\n");
    std::string response = client.recv_response();

    EXPECT_EQ(status_of(response), 200);
    EXPECT_EQ(body_of(response), "hello world");
}

TEST_F(IntegrationTest, BodySplitFromHeadersIsReassembled)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    const std::string body = "split-body";
    client.send_all("POST /echo HTTP/1.1\r\nContent-Length: " +
                    std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n");
    std::this_thread::sleep_for(100ms);
    client.send_all(body);

    std::string response = client.recv_response();
    EXPECT_EQ(status_of(response), 200);
    EXPECT_EQ(body_of(response), body);
}

TEST_F(IntegrationTest, PipelinedRequestsGetBothResponses)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    // Two requests in a single write; second one closes the connection
    client.send_all(
        "GET /hello HTTP/1.1\r\nHost: t\r\n\r\n"
        "GET /no-such-route HTTP/1.1\r\nConnection: close\r\n\r\n");

    std::string first = client.recv_response();
    std::string second = client.recv_response();

    EXPECT_EQ(status_of(first), 200);
    EXPECT_EQ(body_of(first), "hello world");
    EXPECT_EQ(status_of(second), 404);
}

TEST_F(IntegrationTest, KeepAliveServesSequentialRequests)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("GET /hello HTTP/1.1\r\nHost: t\r\n\r\n");
    std::string first = client.recv_response();
    EXPECT_EQ(status_of(first), 200);
    EXPECT_NE(first.find("Connection: keep-alive\r\n"), std::string::npos);

    client.send_all("GET /hello HTTP/1.1\r\nConnection: close\r\n\r\n");
    std::string second = client.recv_response();
    EXPECT_EQ(status_of(second), 200);
    EXPECT_NE(second.find("Connection: close\r\n"), std::string::npos);
}

TEST_F(IntegrationTest, ConnectionCloseIsHonoredWithSocketClose)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("GET /hello HTTP/1.1\r\nConnection: close\r\n\r\n");
    std::string response = client.recv_response();
    ASSERT_EQ(status_of(response), 200);

    EXPECT_TRUE(client.peer_closed()) << "server kept the connection open after Connection: close";
}

TEST_F(IntegrationTest, DeleteOnGetOnlyRouteReturns405)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("DELETE /hello HTTP/1.1\r\nConnection: close\r\n\r\n");
    std::string response = client.recv_response();

    EXPECT_EQ(status_of(response), 405);
    EXPECT_NE(response.find("Allow: GET\r\n"), std::string::npos) << response;
}

// ==========================================================
// Malformed input and safety limits
// ==========================================================

TEST_F(IntegrationTest, MalformedRequestLineReturns400)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("GARBAGE-NOT-HTTP\r\n\r\n");

    EXPECT_EQ(status_of(client.recv_response()), 400);
}

TEST_F(IntegrationTest, OversizedHeadersReturn431)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    // Short request line, then >16 KB of header bytes without a terminator
    std::string request = "GET / HTTP/1.1\r\nX-Big: " + std::string(20 * 1024, 'a');
    client.send_all(request); // May fail mid-send once the server rejects — fine

    EXPECT_EQ(status_of(client.recv_response()), 431);
    EXPECT_TRUE(client.peer_closed());
}

TEST_F(IntegrationTest, OversizedUriReturns414)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    // A ~12 KB request line (over the 8 KB URI limit) followed by filler
    // headers pushing the buffer past the 16 KB header limit
    std::string request = "GET /" + std::string(12 * 1024, 'a') + " HTTP/1.1\r\n" +
                          "X-Filler: " + std::string(8 * 1024, 'b') + "\r\n";
    client.send_all(request);

    EXPECT_EQ(status_of(client.recv_response()), 414);
    EXPECT_TRUE(client.peer_closed());
}

TEST_F(IntegrationTest, OversizedBodyDeclarationReturns413)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("POST /echo HTTP/1.1\r\nContent-Length: 10485761\r\n\r\n");

    EXPECT_EQ(status_of(client.recv_response()), 413);
}

TEST_F(IntegrationTest, ChunkedTransferEncodingReturns501)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("POST /echo HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n");

    EXPECT_EQ(status_of(client.recv_response()), 501);
}

TEST_F(IntegrationTest, DuplicateContentLengthReturns400)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("POST /echo HTTP/1.1\r\nContent-Length: 3\r\nContent-Length: 3\r\n\r\nabc");

    EXPECT_EQ(status_of(client.recv_response()), 400);
}

TEST_F(IntegrationTest, NonNumericContentLengthReturns400)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("POST /echo HTTP/1.1\r\nContent-Length: abc\r\n\r\n");

    EXPECT_EQ(status_of(client.recv_response()), 400);
}

TEST_F(IntegrationTest, NegativeContentLengthReturns400)
{
    TestClient client(port);
    ASSERT_TRUE(client.connected());

    client.send_all("POST /echo HTTP/1.1\r\nContent-Length: -5\r\n\r\n");

    EXPECT_EQ(status_of(client.recv_response()), 400);
}

// ==========================================================
// Disconnects
// ==========================================================

TEST_F(IntegrationTest, MidBodyDisconnectDoesNotKillServer)
{
    {
        TestClient client(port);
        ASSERT_TRUE(client.connected());
        // Promise 100 bytes, deliver 10, then vanish
        client.send_all("POST /echo HTTP/1.1\r\nContent-Length: 100\r\n\r\n0123456789");
    } // Destructor closes the socket mid-request

    // Give the server a moment to hit the dropped connection
    std::this_thread::sleep_for(100ms);

    TestClient next(port);
    ASSERT_TRUE(next.connected());
    next.send_all("GET /hello HTTP/1.1\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(status_of(next.recv_response()), 200);
}

TEST_F(IntegrationTest, ImmediateDisconnectDoesNotKillServer)
{
    {
        TestClient client(port);
        ASSERT_TRUE(client.connected());
    } // Connect and close without sending a byte

    TestClient next(port);
    ASSERT_TRUE(next.connected());
    next.send_all("GET /hello HTTP/1.1\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(status_of(next.recv_response()), 200);
}

TEST_F(IntegrationTest, ManyConcurrentClients)
{
    constexpr int client_count = 16;
    std::atomic<int> ok_count{0};

    std::vector<std::thread> threads;
    threads.reserve(client_count);
    for (int i = 0; i < client_count; ++i)
    {
        threads.emplace_back([&]
        {
            TestClient client(port);
            if (!client.connected())
                return;
            client.send_all("GET /hello HTTP/1.1\r\nConnection: close\r\n\r\n");
            std::string response = client.recv_response();
            if (status_of(response) == 200 && body_of(response) == "hello world")
                ok_count.fetch_add(1);
        });
    }
    for (auto& t : threads)
        t.join();

    EXPECT_EQ(ok_count.load(), client_count);
}
