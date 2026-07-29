#include "handlers/StaticHandler.hpp"
#include "utils/FileSystem.hpp"
#include "utils/Logger.hpp" // For logging information about cached files

#include <string>
#include <unordered_map>
#include <filesystem>
#include <algorithm> // For std::replace


namespace fs = std::filesystem;

namespace handlers 
{
    
    // Structure for storing a cached file
    struct CachedFile 
    {
        std::string content;
        std::string mime_type;
    };

    // Our in-memory cache. It is populated only once at server startup,
    // so reading from it by multiple threads is safe.
    static std::unordered_map<std::string, CachedFile> s_file_cache;


    void init_static_cache(const std::string& public_dir)
    {
        if (!fs::exists(public_dir) || !fs::is_directory(public_dir)) 
        {
            LOG_ERROR("Static directory '{}' not found!", public_dir);
            return;
        }

        // Recursively iterate all files in the folder (e.g., ../public)
        for (const auto& entry : fs::recursive_directory_iterator(public_dir)) 
        {
            if (entry.is_regular_file()) 
            {
                std::string filepath = entry.path().string();
                
            // Turn the path into a URI route
            // For example: "../public/css/style.css" -> "/css/style.css"
            std::string route = filepath.substr(public_dir.length());
                
            // Normalize backslashes to forward slashes for Windows compatibility
            std::replace(route.begin(), route.end(), '\\', '/');

                std::string content;
                if (utils::read_file(filepath, content)) 
                {
                    s_file_cache[route] = { std::move(content), utils::get_mime_type(filepath) };
                    LOG_INFO("Cached static file: {}", route);
                }
            }
        }
        
        // Create an alias for the server root
        if (s_file_cache.find("/index.html") != s_file_cache.end()) 
        {
            s_file_cache["/"] = s_file_cache["/index.html"];
            LOG_INFO("Cached static file: / (alias for /index.html)");
        }
    }

    http::Response handle_static_request(const http::Request& req) 
    {
        // Extract the arena (memory resource) from the request
        std::pmr::memory_resource* mr = req.uri.get_allocator().resource();
        http::Response res(mr); // Bind the response to the same arena

        // Static files can only be fetched: DELETE/POST/etc. must not serve content
        if (req.method != http::Method::GET)
        {
            res.status_code = http::StatusCode::METHOD_NOT_ALLOWED;
            res.body = "<h1>405 Method Not Allowed</h1>";
            res.headers["Allow"] = "GET";
            res.headers["Content-Type"] = "text/html";
            res.headers["Content-Length"] = std::to_string(res.body.size());
            return res;
        }

        if (req.uri.find("..") != std::string_view::npos)
        {
            res.status_code = http::StatusCode::FORBIDDEN;
            res.body = "<h1>403 Forbidden</h1>";
            res.headers["Content-Type"] = "text/html";
            res.headers["Content-Length"] = std::to_string(res.body.size());
            return res;
        }

        // Temporarily convert pmr::string to std::string for cache lookup
        std::string uri_str(req.uri.data(), req.uri.size());

        // Look up the file in memory (O(1) time)
        auto it = s_file_cache.find(uri_str);
        if (it != s_file_cache.end()) 
        {
            res.status_code = http::StatusCode::OK;
            // Copy bytes directly into our fast pmr arena
            res.body = it->second.content; 
            res.headers["Content-Type"] = it->second.mime_type;
        } 
        else 
        {
            res.status_code = http::StatusCode::NOT_FOUND;
            res.body = "<h1>404 - File Not Found</h1>";
            res.headers["Content-Type"] = "text/html";
        }

        res.headers["Content-Length"] = std::to_string(res.body.size());
        return res;
    }

} // namespace handlers