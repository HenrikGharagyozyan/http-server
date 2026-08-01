#pragma once

#include <string>
#include <cstdint>
#include <cstddef>

namespace server 
{

    struct Config 
    {
        uint16_t port = 8080;
        size_t threads = 4;
        std::string public_dir = "./public";

        // Safety limits and timeouts
        size_t max_request_size = 1024 * 1024; // 1 MB
        size_t max_header_size = 16 * 1024;    // 16 KB
        size_t keep_alive_timeout = 5;         // 5 seconds

        // Factory method to load configuration
        static Config load(int argc, char* argv[]);
    };
    
}