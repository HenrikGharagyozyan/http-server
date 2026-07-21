#include "server/TcpServer.hpp"
#include <iostream>

int main() 
{
    try 
    {
        server::TcpServer server;
        uint16_t port = 8080;
        
        std::cout << "Starting server on port " << port << "...\n";
        server.start(port);
        std::cout << "Server is listening. Press Ctrl+C to stop.\n\n";

        // Infinite loop for handling clients
        while (true) 
        {
            std::cout << "Waiting for connection...\n";
            server::Socket client = server.accept_connection();
            
            std::cout << "Client connected! Client fd: " << client.get() << "\n";
            
            // The client socket will go out of scope and be closed by ~Socket(),
            // so we disconnect the client immediately, but the connection itself is successfully tested!
        }
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}