#include "handlers/StaticHandler.hpp"
#include "utils/FileSystem.hpp"
#include <string>

namespace handlers 
{

    http::Response handle_static_request(const http::Request& req) 
    {
        // Extract the arena (memory resource) from the request
        std::pmr::memory_resource* mr = req.uri.get_allocator().resource();
        http::Response res(mr); // Bind the response to the same arena
        
        if (req.uri.find("..") != std::string_view::npos) 
        {
            res.status_code = http::StatusCode::FORBIDDEN;
            res.body = "<h1>403 Forbidden</h1>";
            res.headers["Content-Type"] = "text/html";
            res.headers["Content-Length"] = std::to_string(res.body.size());
            return res;
        }

        std::string filepath = "../public"; 
        if (req.uri == "/") 
        {
            filepath += "/index.html";
        } 
        else 
        {
            filepath += std::string(req.uri); 
        }

        std::string file_content;
        if (utils::read_file(filepath, file_content)) 
        {
            res.status_code = http::StatusCode::OK;
            res.body = std::move(file_content); 
            res.headers["Content-Type"] = utils::get_mime_type(filepath);
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