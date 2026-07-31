#include "server/HttpServer.hpp"
#include "handlers/StaticHandler.hpp"
#include "handlers/UserHandler.hpp"
#include "utils/Logger.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <csignal>

using json = nlohmann::json;

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

int main() 
{
    // 1. Ignore SIGPIPE at the process-wide level.
    // If the client breaks the connection, the process won't crash and send() returns EPIPE.
    std::signal(SIGPIPE, SIG_IGN);

    // 2. Register handlers for clean shutdown
    std::signal(SIGINT, signal_handler);  // Ctrl+C
    std::signal(SIGTERM, signal_handler); // System/container shutdown signal

    try 
    {
        // 1. Read configuration
        std::ifstream config_file("../config.json"); // Path depends on the working directory (build)
        if (!config_file.is_open()) 
        {
            LOG_ERROR("Could not open config.json!");
            return 1;
        }
        
        json config = json::parse(config_file);
        uint16_t port = config["server"]["port"];
        size_t threads = config["server"]["threads"];

        // 2. Initialize the server
        server::HttpServer app;
        g_app = &app; // Pass reference to the global pointer
        
        // Load all static files from disk into memory
        handlers::StaticHandler static_handler("./public");

        app.get("/api/users", handlers::get_users);
        app.post("/api/users", handlers::create_user);
        app.set_default_handler([&static_handler](const http::Request& req)
            {
                return static_handler.handle(req);
            });

        // 3. Start the server with config parameters
        app.listen(port, threads);
    } 
    catch (const json::parse_error& e) 
    {
        LOG_ERROR("Config parse error: {}", e.what());
        return 1;
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}