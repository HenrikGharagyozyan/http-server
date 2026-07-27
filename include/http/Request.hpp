#pragma once

#include <string>
#include <unordered_map>
#include <memory_resource>

namespace http 
{

    // We use enum class for strict typing. 
    // Protects us from accidentally comparing the method with a plain int.
    enum class Method 
    {
        GET,
        POST,
        PUT,
        DELETE,
        UNKNOWN
    };

    struct Request 
    {
        Method method{ Method::UNKNOWN };
        std::pmr::string uri;
        std::pmr::string version;
        
        // Hash table for headers (e.g., "Host" -> "localhost:8080")
        std::pmr::unordered_map<std::pmr::string, std::pmr::string> headers;
        
        std::pmr::string body;

        // Constructor that binds all PMR fields to the provided allocator (arena)
        explicit Request(std::pmr::memory_resource* mr = std::pmr::get_default_resource())
            : uri(mr), version(mr), headers(mr), body(mr) 
        {
        }

    };

} // namespace http