#pragma once

#include <string>
#include <unordered_map>
#include <memory_resource>


namespace http 
{

    enum class StatusCode
    {
        OK                              = 200,
        BAD_REQUEST                     = 400,
        FORBIDDEN                       = 403,
        NOT_FOUND                       = 404,
        PAYLOAD_TOO_LARGE               = 413,
        URI_TOO_LONG                    = 414,
        REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
        INTERNAL_SERVER_ERROR           = 500,
        NOT_IMPLEMENTED                 = 501
    };

    // Helper function to avoid writing "OK" or "Not Found" manually
    std::string get_status_message(StatusCode code);

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