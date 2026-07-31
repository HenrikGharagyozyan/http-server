#pragma once

#include "http/Request.hpp"
#include "http/Response.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

namespace handlers 
{

    // Structure for storing a cached file
    struct CachedFile 
    {
        std::string content;
        std::string mime_type;
    };

    // Transparent hasher for C++20, allowing std::string_view lookups
    // in unordered_map<std::string, ...> without creating temporary std::string
    struct StringHash 
    {
        using is_transparent = void; // <--- Critical for zero-cost string_view lookups

        std::size_t operator()(std::string_view sv) const 
        {
            return std::hash<std::string_view>{}(sv);
        }
    };
    class StaticHandler 
    {
    public:
        // Constructor automatically reads and caches the specified directory
        explicit StaticHandler(const std::string& public_dir);

        // Main method for handling HTTP requests
        http::Response handle(const http::Request& req) const;

    private:
        void load_cache(const std::string& public_dir);

        // Hash table with heterogeneous lookup support (is_transparent)
        using CacheMap = std::unordered_map<std::string, CachedFile, StringHash, std::equal_to<>>;
        CacheMap m_cache;
    };

} // namespace handlers