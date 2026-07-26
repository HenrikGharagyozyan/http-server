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

    Request parse_request(std::string_view raw_data, std::pmr::memory_resource* mr)
    {
        // Инициализируем Request нашим PMR-ресурсом
        Request req(mr);

        // ==========================================================
        // Find the end of headers
        // ==========================================================
        const size_t headers_end = raw_data.find("\r\n\r\n");
        if (headers_end == std::string_view::npos)
        {
            throw std::invalid_argument("Invalid HTTP request");
        }

        std::string_view headers = raw_data.substr(0, headers_end);
        
        // Аллоцируем body в арене
        req.body = std::pmr::string(raw_data.substr(headers_end + 4), mr);

        // ==========================================================
        // Parse request line
        // ==========================================================
        size_t line_end = headers.find("\r\n");
        if (line_end == std::string_view::npos)
        {
            throw std::invalid_argument("Invalid HTTP request line");
        }

        std::string_view request_line = headers.substr(0, line_end);

        size_t method_end = request_line.find(' ');
        size_t uri_end = request_line.find(' ', method_end + 1);

        if (method_end == std::string_view::npos ||
            uri_end == std::string_view::npos)
        {
            throw std::invalid_argument("Invalid HTTP request line");
        }

        req.method = string_to_method(request_line.substr(0, method_end));
        
        // Аллоцируем URI и версию в арене
        req.uri = std::pmr::string(request_line.substr(method_end + 1, uri_end - method_end - 1), mr);
        req.version = std::pmr::string(request_line.substr(uri_end + 1), mr);

        // ==========================================================
        // Parse headers
        // ==========================================================
        size_t start = line_end + 2;

        while (start < headers.size())
        {
            line_end = headers.find("\r\n", start);

            if (line_end == std::string_view::npos)
            {
                line_end = headers.size();
            }

            std::string_view header_line = headers.substr(start, line_end - start);

            if (!header_line.empty())
            {
                size_t colon = header_line.find(':');

                if (colon != std::string_view::npos)
                {
                    std::string_view key = header_line.substr(0, colon);
                    std::string_view value = header_line.substr(colon + 1);

                    while (!value.empty() && value.front() == ' ')
                    {
                        value.remove_prefix(1);
                    }

                    // Аллоцируем ключи и значения заголовков в арене перед вставкой в unordered_map
                    req.headers.emplace(
                        std::pmr::string(key, mr), 
                        std::pmr::string(value, mr)
                    );
                }
            }

            start = line_end + 2;
        }

        return req;
    }

} // namespace http