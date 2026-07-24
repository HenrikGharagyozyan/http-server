#include "server/HttpServer.hpp"
#include "handlers/StaticHandler.hpp"
#include "handlers/UserHandler.hpp"

#include <iostream>


int main() 
{
    try 
    {
        server::HttpServer app;
        
        // 1. GET /api/users — Returns a list of users in JSON format
        app.get("/api/users", handlers::get_users);

        // 2. POST /api/users — Create a new user with JSON validation
        app.post("/api/users", handlers::create_user);

        // Static file handler (fallback)
        app.set_default_handler(handlers::handle_static_request);

        // Run server
        app.listen(8080);
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}