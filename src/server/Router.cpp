#include "server/Router.hpp"

namespace server 
{

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
        // 1. Find the method (GET, POST, etc.)
        auto method_it = routes_.find(req.method);
        if (method_it != routes_.end()) 
        {
            // 2. Find the specific URI (e.g., "/users")
            auto uri_it = method_it->second.find(req.uri);
            if (uri_it != method_it->second.end()) 
            {
                // Found! Call the handler function, passing it the Request
                return uri_it->second(req); 
            }
        }

        if (default_handler_)
            return default_handler_(req);

        // If no route is found, return 404
        http::Response res;
        res.status_code = http::StatusCode::NOT_FOUND;
        res.body = "404 Not Found\n";
        res.headers["Content-Length"] = std::to_string(res.body.size());
        res.headers["Content-Type"] = "text/plain";
        res.headers["Connection"] = "close";
        return res;
    }

} // namespace server