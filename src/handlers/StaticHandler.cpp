#include "handlers/StaticHandler.hpp"
#include "utils/FileSystem.hpp"
#include "utils/Logger.hpp"

#include <filesystem>
#include <algorithm> // For std::replace

namespace fs = std::filesystem;

namespace handlers 
{

    StaticHandler::StaticHandler(const std::string& public_dir)
    {
        load_cache(public_dir);
    }

    void StaticHandler::load_cache(const std::string& public_dir)
    {
        if (!fs::exists(public_dir) || !fs::is_directory(public_dir)) 
        {
            LOG_ERROR("Static directory '{}' not found!", public_dir);
            return;
        }

        // Recursively walk all files in the directory
        for (const auto& entry : fs::recursive_directory_iterator(public_dir)) 
        {
            if (entry.is_regular_file()) 
            {
                std::string filepath = entry.path().string();
                
                // Turn the local file path into a URI route
                std::string route = filepath.substr(public_dir.length());
                
                // Normalize backslashes for Windows
                std::replace(route.begin(), route.end(), '\\', '/');

                std::string content;
                if (utils::read_file(filepath, content)) 
                {
                    std::string mime = utils::get_mime_type(filepath);
                    m_cache.emplace(route, CachedFile{ std::move(content), std::move(mime) });
                    LOG_INFO("Cached static file: {}", route);
                }
            }
        }
        
        // Alias for the server root
        if (auto it = m_cache.find("/index.html"); it != m_cache.end()) 
        {
            m_cache["/"] = it->second;
            LOG_INFO("Cached static file: / (alias for /index.html)");
        }
    }

    http::Response StaticHandler::handle(const http::Request& req) const 
    {
        // Get the memory_resource from the request
        std::pmr::memory_resource* mr = req.uri.get_allocator().resource();
        http::Response res(mr);

        // 1. Serve static files only for GET requests
        if (req.method != http::Method::GET)
        {
            res.status_code = http::StatusCode::METHOD_NOT_ALLOWED;
            res.body = "<h1>405 Method Not Allowed</h1>";
            res.headers["Allow"] = "GET";
            res.headers["Content-Type"] = "text/html";
            return res;
        }

        // 2. Protect against Directory Traversal (attempts to escape the directory)
        if (req.uri.find("..") != std::string_view::npos)
        {
            res.status_code = http::StatusCode::FORBIDDEN;
            res.body = "<h1>403 Forbidden</h1>";
            res.headers["Content-Type"] = "text/html";
            return res;
        }

        // 3. Lookup in the cache WITHOUT allocations thanks to StringHash and std::string_view!
        auto it = m_cache.find(std::string_view(req.uri));
        if (it != m_cache.end()) 
        {
            res.status_code = http::StatusCode::OK;
            res.body = it->second.content; 
            res.headers["Content-Type"] = it->second.mime_type;
        } 
        else 
        {
            res.status_code = http::StatusCode::NOT_FOUND;
            res.body = "<h1>404 - File Not Found</h1>";
            res.headers["Content-Type"] = "text/html";
        }

        return res;
    }

} // namespace handlers