#pragma once

#include "http/Request.hpp"
#include "http/Response.hpp"
#include <functional>
#include <unordered_map>
#include <string>
#include <string_view>


namespace server 
{

    // Define a type alias for the request handler
    using Handler = std::function<http::Response(const http::Request&)>;

    
    // C++20 Heterogeneous Lookup: Transparent hasher
    // Позволяет использовать std::string_view для поиска в unordered_map<std::string, ...>
    struct StringHash 
    {
        using is_transparent = void; // Магический флаг, включающий прозрачный поиск

        std::size_t operator()(std::string_view sv) const 
        {
            return std::hash<std::string_view>{}(sv);
        }
    };


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
        // Используем наш StringHash и std::equal_to<> для поиска без аллокаций
        std::unordered_map<
            http::Method, 
            std::unordered_map<std::string, Handler, StringHash, std::equal_to<>>
        > routes_;

        Handler default_handler_{nullptr};
    };

} // namespace server