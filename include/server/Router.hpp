#pragma once

#include "http/Request.hpp"
#include "http/Response.hpp"
#include <functional>
#include <unordered_map>
#include <string>

namespace server 
{

    // Define a type alias for the request handler
    using Handler = std::function<http::Response(const http::Request&)>;

    class Router 
    {
    public:
        Router() = default;

        // Methods for registering routes
        void get(const std::string& uri, Handler handler);
        void post(const std::string& uri, Handler handler);

        // Main dispatch method: finds the appropriate Handler and invokes it
        [[nodiscard]] http::Response route(const http::Request& req) const;

        void set_default_handler(Handler handler);

    private:
        // Two-level hash table: Method -> (URI -> Handler)
        std::unordered_map<http::Method, std::unordered_map<std::string, Handler>> routes_;

        Handler default_handler_{nullptr};
    };

} // namespace server