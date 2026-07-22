#pragma once

#include <string>
#include <unordered_map>

namespace http 
{

    enum class StatusCode
    {
        OK = 200,
        BAD_REQUEST = 400,
        NOT_FOUND = 404,
        INTERNAL_SERVER_ERROR = 500
    };

    // Вспомогательная функция, чтобы не писать "OK" или "Not Found" руками
    std::string get_status_message(StatusCode code);

    struct Response 
    {
        StatusCode status_code{ StatusCode::OK };
        
        std::unordered_map<std::string, std::string> headers;
        std::string body;

        // Converts the Response structure back to text HTTP format for sending to the socket
        [[nodiscard]] std::string serialize() const;
    };

} // namespace http