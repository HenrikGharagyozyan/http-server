#include "http/Parser.hpp"

#include <stdexcept>

namespace http 
{

    // Helper function to convert string to enum Method
    static Method string_to_method(std::string_view method_str) 
    {
        if (method_str == "GET") return Method::GET;
        if (method_str == "POST") return Method::POST;
        if (method_str == "PUT") return Method::PUT;
        if (method_str == "DELETE") return Method::DELETE;
        return Method::UNKNOWN;
    }

    Request parse_request(std::string_view raw_data) 
    {
        Request req;
        
        // 1. Find the end of the first line (Request-Line)
        size_t line_end = raw_data.find("\r\n");
        if (line_end == std::string_view::npos) 
        {
            throw std::invalid_argument("Invalid HTTP request: no request line");
        }
        
        std::string_view request_line = raw_data.substr(0, line_end);
        
        // Parse method, URI, and version from request_line (they are separated by spaces)
        size_t method_end = request_line.find(' ');
        size_t uri_end = request_line.find(' ', method_end + 1);
        
        if (method_end == std::string_view::npos || uri_end == std::string_view::npos) 
        {
            throw std::invalid_argument("Invalid HTTP request line format");
        }
        
        req.method = string_to_method(request_line.substr(0, method_end));
        req.uri = std::string(request_line.substr(method_end + 1, uri_end - method_end - 1));
        req.version = std::string(request_line.substr(uri_end + 1));

        // 2. Parse headers
        size_t start = line_end + 2; // skip \r\n
        while (true) 
        {
            line_end = raw_data.find("\r\n", start);
            if (line_end == std::string_view::npos) 
                break;
            
            // If we found an empty line (\r\n\r\n), headers are finished
            if (line_end == start) 
            {
                start += 2; // jump over the empty line to the body
                break;
            }
            
            std::string_view header_line = raw_data.substr(start, line_end - start);
            size_t colon_pos = header_line.find(':');
            
            if (colon_pos != std::string_view::npos) 
            {
                std::string_view key = header_line.substr(0, colon_pos);
                std::string_view value = header_line.substr(colon_pos + 1);
                
                // Remove leading space from the value (e.g., "Host: localhost" -> "localhost")
                if (!value.empty() && value[0] == ' ') 
                {
                    value.remove_prefix(1);
                }
                
                req.headers[std::string(key)] = std::string(value);
            }
            
            start = line_end + 2;
        }

        // 3. Everything else is the request body (Body)
        if (start < raw_data.size()) 
        {
            req.body = std::string(raw_data.substr(start));
        }

        return req;
    }

} // namespace http