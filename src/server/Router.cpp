#include "server/Router.hpp"

namespace server
{

    // Helper for building the Allow header of a 405 response
    static std::string_view method_name(http::Method method)
    {
        switch (method)
        {
            case http::Method::GET:    return "GET";
            case http::Method::POST:   return "POST";
            case http::Method::PUT:    return "PUT";
            case http::Method::DELETE: return "DELETE";
            default:                   return "";
        }
    }

    void Router::get(const std::string& uri, Handler handler)
    {
        // std::move prevents unnecessary copying of the function object
        routes_[http::Method::GET][uri] = std::move(handler);
    }

    void Router::post(const std::string& uri, Handler handler) 
    {
        routes_[http::Method::POST][uri] = std::move(handler);
    }

    void server::Router::set_default_handler(Handler handler)
    {
        default_handler_ = std::move(handler);
    }

    http::Response Router::route(const http::Request& req) const
    {
        // Convert pmr::string to a string_view for dictionary lookup
        std::string_view uri_sv(req.uri.data(), req.uri.size());

        // Extract the memory resource (our arena) from the incoming request
        std::pmr::memory_resource* mr = req.uri.get_allocator().resource();

        // 1. Find the method (GET, POST, etc.)
        auto method_it = routes_.find(req.method);
        if (method_it != routes_.end())
        {
            // 2. Find the specific URI (e.g., "/users")
            // Thanks to C++20 heterogeneous lookup, this find does not allocate memory!
            auto uri_it = method_it->second.find(uri_sv);

            if (uri_it != method_it->second.end())
            {
                // Found! Call the handler function, passing it the Request
                return uri_it->second(req);
            }
        }

        // 3. The URI is registered under a different method: this must be
        // 405 Method Not Allowed, never a fallthrough to the default handler
        // (otherwise DELETE /index.html would be served by the static handler).
        std::pmr::string allow(mr);
        for (const auto& [method, uri_map] : routes_)
        {
            if (uri_map.find(uri_sv) != uri_map.end())
            {
                if (!allow.empty())
                    allow += ", ";
                allow += method_name(method);
            }
        }

        if (!allow.empty())
        {
            http::Response res(mr);
            res.status_code = http::StatusCode::METHOD_NOT_ALLOWED;
            res.body = "405 Method Not Allowed\n";
            res.headers["Allow"] = allow;
            res.headers["Content-Length"] = std::to_string(res.body.size());
            res.headers["Content-Type"] = "text/plain";
            res.headers["Connection"] = "close";
            return res;
        }

        if (default_handler_)
            return default_handler_(req);

        // 4. If no route is found, return 404 (allocating it inside the Arena!)
        // Create a Response bound to the same arena
        http::Response res(mr);
        res.status_code = http::StatusCode::NOT_FOUND;
        res.body = "404 Not Found\n";

        // Hash table keys and values will also automatically use the arena
        res.headers["Content-Length"] = std::to_string(res.body.size());
        res.headers["Content-Type"] = "text/plain";
        res.headers["Connection"] = "close";

        return res;
    }

} // namespace server