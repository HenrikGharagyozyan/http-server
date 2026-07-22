#pragma once

#include <string>
#include <unordered_map>

namespace http 
{

    struct Response 
    {
        int status_code{ 200 };
        std::string status_message{ "OK" };
        
        std::unordered_map<std::string, std::string> headers;
        std::string body;

        // Converts the Response structure back to text HTTP format for sending to the socket
        [[nodiscard]] std::string serialize() const;
    };

} // namespace http