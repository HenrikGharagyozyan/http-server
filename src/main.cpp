#include "server/HttpServer.hpp"
#include "server/Config.hpp"
#include "handlers/StaticHandler.hpp"
#include "handlers/UserHandler.hpp"
#include "utils/Logger.hpp"

#include <csignal>
#include <exception>


// Global pointer is needed to catch OS signals
server::HttpServer* g_app = nullptr;

// Signal handler (called on Ctrl+C or kill command)
// Signal handlers must only call async-signal-safe operations.
// Logging (spdlog) is forbidden here because of mutex deadlock risk.
void signal_handler(int /*signal_num*/) 
{
    if (g_app) 
    {
        g_app->stop();
    }
}

int main(int argc, char* argv[])
{
    // 1. Ignore SIGPIPE at the process-wide level.
    // If the client breaks the connection, the process won't crash and send() returns EPIPE.
    std::signal(SIGPIPE, SIG_IGN);

    // 2. Register handlers for clean shutdown
    std::signal(SIGINT, signal_handler);  // Ctrl+C
    std::signal(SIGTERM, signal_handler); // System/container shutdown signal

    try 
    {
        // 1. Загружаем конфигурацию (Файл -> Аргументы -> Env -> Дефолт + Валидация)
        server::Config config = server::Config::load(argc, argv);

        // 2. Load all static files from disk into memory
        handlers::StaticHandler static_handler(config.public_dir);

        // 3. Инициализируем сервер
        server::HttpServer app;
        g_app = &app;
        

        app.get("/api/users", handlers::get_users);
        app.post("/api/users", handlers::create_user);
        app.set_default_handler([&static_handler](const http::Request& req)
            {
                return static_handler.handle(req);
            });

        // 3. Start the server with config parameters
        LOG_INFO("Starting HTTP server on port {} with {} threads", config.port, config.threads);
        app.listen(config);
    } 
    catch (const std::exception& e) 
    {
        LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}