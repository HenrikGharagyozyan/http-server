#include "http/Response.hpp"

#include <sstream> // For std::ostringstream

namespace http 
{

    std::string get_status_message(StatusCode code) 
    {
        switch (code) 
        {
            case StatusCode::OK:                              return "OK";
            case StatusCode::BAD_REQUEST:                     return "Bad Request";
            case StatusCode::FORBIDDEN:                       return "Forbidden";
            case StatusCode::NOT_FOUND:                       return "Not Found";
            case StatusCode::PAYLOAD_TOO_LARGE:               return "Payload Too Large";
            case StatusCode::URI_TOO_LONG:                    return "URI Too Long";
            case StatusCode::REQUEST_HEADER_FIELDS_TOO_LARGE: return "Request Header Fields Too Large";
            case StatusCode::INTERNAL_SERVER_ERROR:           return "Internal Server Error";
            case StatusCode::NOT_IMPLEMENTED:                 return "Not Implemented";
            default:                                          return "Unknown";
        }
    }

    std::string Response::serialize() const 
    {
        std::ostringstream oss;
        
        // 1. Status line
        // Format: HTTP/1.1 <Code> <Message>\r\n
        oss << "HTTP/1.1 " << static_cast<int>(status_code) << " " 
            << get_status_message(status_code) << "\r\n";
        
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
