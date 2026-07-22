#pragma once

#include <string>
#include <unordered_map>

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
        std::string uri;
        std::string version;
        
        // Hash table for headers (e.g., "Host" -> "localhost:8080")
        std::unordered_map<std::string, std::string> headers;
        
        std::string body;
    };

} // namespace http