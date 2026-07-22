#include "http/Response.hpp"

#include <sstream> // For std::ostringstream

namespace http 
{

    std::string Response::serialize() const 
    {
        std::ostringstream oss;
        
        // 1. Status line
        // Format: HTTP/1.1 <Code> <Message>\r\n
        oss << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
        
        // 2. Headers
        // Format: <Key>: <Value>\r\n
        for (const auto& [key, value] : headers) 
        {
            oss << key << ": " << value << "\r\n";
        }
        
        // 3. Mandatory empty line (\r\n) separating headers from body
        oss << "\r\n";
        
        // 4. Response body
        oss << body;
        
        return oss.str();
    }

} // namespace http