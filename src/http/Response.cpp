#include "http/Response.hpp"

#include <sstream> // For std::ostringstream

namespace http 
{

    std::string Response::serialize() const 
    {
        std::string result;
        result.reserve(256 + body.size());

        // Status Line
        result += "HTTP/1.1 ";
        result += std::to_string(static_cast<int>(status_code));
        result += " ";
        result += get_status_message(status_code);
        result += "\r\n";

        // Check for Content-Length (HeaderMap lookup is case-insensitive)
        bool has_content_length = (headers.find("Content-Length") != headers.end());

        // Headers
        for (const auto& [key, value] : headers) 
        {
            result.append(key.data(), key.size());
            result += ": ";
            result.append(value.data(), value.size());
            result += "\r\n";
        }

        // Automatically add Content-Length if the handler did not set it
        // (except for 204 No Content and 304 Not Modified)
        if (!has_content_length && status_code != StatusCode::NO_CONTENT && status_code != StatusCode::NOT_MODIFIED) 
        {
            result += "Content-Length: ";
            result += std::to_string(body.size());
            result += "\r\n";
        }

        result += "\r\n";
        result.append(body.data(), body.size());

        return result;
    }

} // namespace http
