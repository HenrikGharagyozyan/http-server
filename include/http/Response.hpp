#pragma once

#include "http/HttpStatus.hpp"

#include <string>
#include <unordered_map>
#include <memory_resource>


namespace http 
{

    struct Response 
    {
        StatusCode status_code{ StatusCode::OK };
        
        std::pmr::unordered_map<std::pmr::string, std::pmr::string> headers;
        std::pmr::string body;


        explicit Response(std::pmr::memory_resource* mr = std::pmr::get_default_resource())
            : headers(mr), body(mr) 
        {
        }

        // Converts the Response structure back to text HTTP format for sending to the socket
        [[nodiscard]] std::string serialize() const;
    };

} // namespace http