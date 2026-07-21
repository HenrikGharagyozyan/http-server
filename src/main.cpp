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

        while (true) 
        {
            std::cout << "Waiting for connection...\n";
            server::Socket client = server.accept_connection();
            std::cout << "Client connected! Client fd: " << client.get() << "\n";
            
            // Read the request from the browser or curl
            std::string request = client.recv();
            std::cout << "--- Received HTTP Request ---\n";
            std::cout << request << "\n";
            std::cout << "-----------------------------\n";

            // Build a hard-coded HTTP response
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 13\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Hello, World!";
            
            // Send the response
            client.send(response);
            std::cout << "Response sent.\n\n";
            
            // At the end of the iteration, ~Socket() will run and close the connection
        }
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}